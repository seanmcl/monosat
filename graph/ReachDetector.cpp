/*
 * ReachDetector.c
 *
 *  Created on: 2014-01-08
 *      Author: sam
 */



#include "ReachDetector.h"
#include "dgl/UnweightedRamalReps.h"
#include "GraphTheory.h"
#include "core/Config.h"
#include "dgl/DynamicConnectivity.h"
#include "dgl/TarjansSCC.h"
using namespace Minisat;
ReachDetector::ReachDetector(int _detectorID, GraphTheorySolver * _outer, DynamicGraph &_g, DynamicGraph &_antig, int from,double seed):Detector(_detectorID),outer(_outer),g(_g),antig(_antig),within(-1),source(from),rnd_seed(seed),positive_reach_detector(NULL),negative_reach_detector(NULL),positive_path_detector(NULL),positiveReachStatus(NULL),negativeReachStatus(NULL),opt_weight(*this),chokepoint_status(*this),chokepoint(chokepoint_status, _antig,source){

	constraintsBuilt=-1;
	rnd_path=nullptr;
	opt_path=nullptr;
	chokepoint_detector=nullptr;
	cutgraph_reach_detector=nullptr;
	 positiveReachStatus=nullptr;
	 negativeReachStatus=nullptr;
	 positive_reach_detector=nullptr;
	 negative_reach_detector=nullptr;
	 positive_path_detector=nullptr;
	first_reach_var = var_Undef;
	stats_pure_skipped=0;
	stats_shrink_removed=0;
	 reach_marker=CRef_Undef;
	 non_reach_marker=CRef_Undef;
	 forced_reach_marker=CRef_Undef;
	if(reachalg==ReachAlg::ALG_SAT){

		return;
	}

	if(opt_decide_graph_chokepoints){
		 chokepoint_detector = new DFSReachability<Reach::NullStatus>(from,_antig,Reach::nullStatus,1);

	}
	if(opt_shrink_theory_conflicts){
		cutgraph_reach_detector= new UnweightedRamalReps<Reach::NullStatus>(from,cutgraph,Reach::nullStatus,0);
	}

	 if(opt_use_random_path_for_decisions){
		 rnd_weight.clear();
		 rnd_path = new WeightedDijkstra< vec<double> >(from,_antig,rnd_weight);
		 for(int i=0;i<outer->edge_list.size();i++){
			 double w = drand(rnd_seed);

			 rnd_weight.push(w);
		 }

	 }

	 if(opt_use_optimal_path_for_decisions){
		 opt_path = new WeightedDijkstra< OptimalWeightEdgeStatus >(from,_antig,opt_weight);
	 }
	 positiveReachStatus = new ReachDetector::ReachStatus(*this,true);
	 negativeReachStatus = new ReachDetector::ReachStatus(*this,false);
	if(reachalg==ReachAlg::ALG_BFS){
		if(!opt_encode_reach_underapprox_as_sat)
		{

			positive_reach_detector = new BFSReachability<ReachDetector::ReachStatus>(from,_g,*(positiveReachStatus),1);
		}


		negative_reach_detector = new BFSReachability<ReachDetector::ReachStatus>(from,_antig,*(negativeReachStatus),-1);
/*		if(opt_conflict_shortest_path)
			positive_path_detector = new Distance<NullEdgeStatus>(from,_g,nullEdgeStatus,1);
		else*/
		positive_path_detector =positive_reach_detector;
	}else if(reachalg==ReachAlg::ALG_DFS){
		if(!opt_encode_reach_underapprox_as_sat)
		{

			positive_reach_detector = new DFSReachability<ReachDetector::ReachStatus>(from,_g,*(positiveReachStatus),1);
		}

		negative_reach_detector = new DFSReachability<ReachDetector::ReachStatus>(from,_antig,*(negativeReachStatus),-1);
		if(opt_conflict_shortest_path)
			positive_path_detector = new Distance<Reach::NullStatus>(from,_g,Reach::nullStatus,1);
		else
			positive_path_detector =positive_reach_detector;
	}else if(reachalg==ReachAlg::ALG_DISTANCE){
		if(!opt_encode_reach_underapprox_as_sat)
		{
			positive_reach_detector = new Distance<ReachDetector::ReachStatus>(from,_g,*(positiveReachStatus),1);
		}

		negative_reach_detector = new Distance<ReachDetector::ReachStatus>(from,_antig,*(negativeReachStatus),-1);
		positive_path_detector = positive_reach_detector;
	}else if(reachalg==ReachAlg::ALG_RAMAL_REPS){
		if(!opt_encode_reach_underapprox_as_sat)
		{
			positive_reach_detector = new UnweightedRamalReps<ReachDetector::ReachStatus>(from,_g,*(positiveReachStatus),1,false);
		}

		negative_reach_detector = new UnweightedRamalReps<ReachDetector::ReachStatus>(from,_antig,*(negativeReachStatus),-1,false);
		positive_path_detector = new Distance<Reach::NullStatus>(from,_g,Reach::nullStatus,1);
	}/*else if (reachalg==ReachAlg::ALG_THORUP){


		positive_reach_detector = new DynamicConnectivity<ReachDetector::ReachStatus>(_g,*(positiveReachStatus),1);
		negative_reach_detector = new DynamicConnectivity<ReachDetector::ReachStatus>(_antig,*(negativeReachStatus),-1);
		positive_path_detector = positive_reach_detector;
		if(opt_conflict_shortest_path)
			positive_path_detector = new Distance<NullEdgeStatus>(from,_g,nullEdgeStatus,1);
		else
			positive_path_detector =positive_reach_detector;
	}*/else{
		if(!opt_encode_reach_underapprox_as_sat)
		{
			positive_reach_detector = new Dijkstra<ReachDetector::ReachStatus>(from,_g,*positiveReachStatus,1);
		}
		negative_reach_detector = new Dijkstra<ReachDetector::ReachStatus>(from,_antig,*negativeReachStatus,-1);
		positive_path_detector = positive_reach_detector;
		//reach_detectors.last()->positive_dist_detector = new Dijkstra(from,g);
	}
	if(positive_reach_detector)
		positive_reach_detector->setSource(source);
	if(negative_reach_detector)
		negative_reach_detector->setSource(source);

	reach_marker=outer->newReasonMarker(getID());
	non_reach_marker=outer->newReasonMarker(getID());
	forced_reach_marker=outer->newReasonMarker(getID());
}
void ReachDetector::buildSATConstraints(bool onlyUnderApprox,int within_steps){
	if(within_steps<0)
		within_steps=g.nodes();
	if(within_steps>g.nodes())
		within_steps=g.nodes();
	if(within_steps>g.edges())
		within_steps=g.edges();
	if(constraintsBuilt>=within_steps)
		return;

	//there is no reason to encode these variables in the theory solver!

	if(!onlyUnderApprox){
		assert(outer->decisionLevel()==0);
		vec<Lit> c;

		if(constraintsBuilt<=0){
			constraintsBuilt=0;
			dist_lits.push();
			Lit True = mkLit(outer->newVar());
			outer->addClause(True);
			assert(outer->value(True)==l_True);
			Lit False = ~True;
			assert(outer->value(False)==l_False);
			for(int i = 0;i<g.nodes();i++){
				dist_lits[0].push(False);
			}
			dist_lits[0][source]=True;
		}

		vec<Lit>  reaches;

		vec<Lit> incomingEdges;
		vec<Lit> incomingNodes;

		//bellman-ford:
		for (int i = constraintsBuilt;i<within_steps;i++){
			dist_lits.last().copyTo(reaches);
			dist_lits.push();
			reaches.copyTo(dist_lits.last());
			assert(outer->value( reaches[source])==l_True);
			//For each edge:
			for(int j = 0;j<g.nodes();j++){
				Lit r_cur = reaches[j];

				for(Edge & e: outer->inv_adj[j]){
					//Edge e = outer->edges[j][k];
					assert(e.to==j);
					if(outer->value(dist_lits.last()[e.to])==l_True){
						//do nothing
					}else if (outer->value(reaches[e.from])==l_False){
						//do nothing
					}else{
						Lit l = mkLit(e.v,false);

						Lit r = mkLit( outer->newVar(), false);

						c.clear();
						c.push(~r);c.push(r_cur);c.push(l); //r -> (e.l or reaches[e.to])
						outer->addClause(c);
						c.clear();
						c.push(~r);c.push(r_cur);c.push(reaches[e.from]); //r -> (reaches[e.from]) or reaches[e.to])
						outer->addClause(c);
						c.clear();
						c.push(r);c.push(~r_cur); //~r -> ~reaches[e.to]
						outer->addClause(c);
						c.clear();
						c.push(r);c.push(~reaches[e.from]);c.push(~l); //~r -> (~reaches[e.from] or ~e.l)
						outer->addClause(c);
						r_cur=r;

					}
				}
				dist_lits.last()[j]=r_cur;
			}
			assert(outer->value( dist_lits.last()[source])==l_True);
		}
		assert(dist_lits.size()==within_steps+1);
		if(within_steps==g.nodes() || within_steps==g.edges()){
			assert(reach_lits.size()==0);
			for(Lit d: dist_lits.last()){
				reach_lits.push(d);
			}
			assert(outer->value( reach_lits[source])==l_True);
		}

		constraintsBuilt=within_steps;
	}else{
		if(constraintsBuilt<0){
		constraintsBuilt=g.nodes();

		//for each node, it cannot be reachable if none of its incoming.edges() are enabled.
		for(int n = 0;n<g.nodes();n++){
			if(reach_lits[n]==lit_Undef){
				Var reach_var = outer->newVar(detectorID , true);
				reach_lits[n] =mkLit(reach_var);//since this is _only_ the underapproximation, these variables _do_ need to be connected to the theory solver
				while(reach_lit_map.size()<= reach_var- first_reach_var ){
					reach_lit_map.push(-1);
				}
				reach_lit_map[reach_var-first_reach_var]=n;

			}
		}

		vec<Lit> c;
		vec<Lit> t;
		for(int n = 0;n<g.nodes();n++){
			if(n==53){
				int a =1;
			}
			if(n==source){
				outer->addClause(reach_lits[n]);//source node is unconditionally reachable
			}else{
				c.clear();
				c.push(~reach_lits[n]);
				//for(auto edge:g.inverted_adjacency[n]){
				for(int i = 0;i<g.nIncoming(n);i++){
					auto & edge = g.incoming(n,i);
					int edgeID = edge.id;
					int from = edge.node;
					if(from!=n){
						//ignore trivial cycles
						Var v = outer->getEdgeVar(edgeID);
						c.push(mkLit(v));
					}
				}

				//Either an edge must be true, or the reach_lit must be false (unless this is the source reach node);
				outer->addClause(c);
				c.clear();
				//either an incoming node must be true, or reach_lit must be false
				c.push(~reach_lits[n]);
				for(int i = 0;i<g.nIncoming(n);i++){
					auto & edge = g.incoming(n,i);
					int edgeID = edge.id;
					int from = edge.node;
					if(from!=n){//ignore trivial cycles
						c.push(reach_lits[from]);
					}
				}
				//Either at least one incoming node must be true, or the reach_lit must be false (unless this is the source reach node);
				outer->addClause(c);

				//Either at least incoming edge AND its corresponding from node must BOTH be simultaneously true, or the node is must not be reachable
				c.clear();
				//either an incoming node must be true, or reach_lit must be false
				c.push(~reach_lits[n]);
				for(int i = 0;i<g.nIncoming(n);i++){
					auto & edge = g.incoming(n,i);
					int edgeID = edge.id;
					int from = edge.node;
					if(from!=n){//ignore trivial cycles
						Lit e =mkLit( outer->getEdgeVar(edgeID));
						Lit incoming = reach_lits[from];

						Lit andGate = mkLit(outer->newVar());
						//Either the andgate is true, or at least one of e, incoming is false
						outer->addClause(andGate,~incoming,~e);
						//If andgate is true, incoming is true
						outer->addClause(~andGate,incoming);
						outer->addClause(~andGate,e);
						c.push(andGate);
					}
				}
				//Either one andGate must be true, or the reach_lit must be false
				outer->addClause(c);
			}

			//If this node is reachable, the for each outgoing edge, if that edge is enabled, its to node must also be reachable
			for(int i = 0;i<g.nIncident(n);i++){
				auto & edge = g.incident(n,i);
				int edgeID = edge.id;
				int to = edge.node;
				if(to!=n){//ignore trivial cycles
					Lit e =mkLit( outer->getEdgeVar(edgeID));
					Lit outgoing = reach_lits[to];
					outer->addClause(~reach_lits[n],~e, outgoing);
				}
			}


		}
		}
	}
}
void ReachDetector::addLit(int from, int to, Var outer_reach_var){
	while( reach_lits.size()<=g.nodes())
			reach_lits.push(lit_Undef);
	if(reach_lits[to]!=lit_Undef){
		Lit r = reach_lits[to];
		//force equality between the new lit and the old reach lit, in the SAT solver
		outer->makeEqualInSolver(outer->toSolver(r),mkLit(outer_reach_var));
		return;
	}

	g.invalidate();
	antig.invalidate();

	Var reach_var = outer->newVar(outer_reach_var,getID());

	if(first_reach_var==var_Undef){
		first_reach_var=reach_var;
	}else{
		assert(reach_var>=first_reach_var);
	}
	Lit reachLit=mkLit(reach_var,false);
	assert(reach_lits[to]==lit_Undef);
	//if(reach_lits[to]==lit_Undef){
	reach_lits[to] = reachLit;
	while(reach_lit_map.size()<= reach_var- first_reach_var ){
		reach_lit_map.push(-1);
	}
	reach_lit_map[reach_var-first_reach_var]=to;

	assert(from==source);
	 if(opt_encode_reach_underapprox_as_sat || !positive_reach_detector){
		 buildSATConstraints(true);
	 }
	 if( !negative_reach_detector){
		 buildSATConstraints(false);
	 }





	/*}else{


		outer->S->addClause(~r, reachLit);
		outer->S->addClause(r, ~reachLit);
	}*/
}

void ReachDetector::ReachStatus::setReachable(int u, bool reachable){
				if(reachable){
					assert(!detector.outer->dbg_notreachable( detector.source,u));
				}else{
					assert(detector.outer->dbg_notreachable( detector.source,u));
				}
				if(polarity==reachable && u<detector.reach_lits.size()){
					Lit l = detector.reach_lits[u];
					if(l!=lit_Undef){
						lbool assign = detector.outer->value(l);
						if(assign!= (reachable? l_True:l_False )){
							detector.changed.push({reachable? l:~l,u});
						}
					}
				}
			}

void ReachDetector::ReachStatus::setMininumDistance(int u, bool reachable, int distance){
	/*assert(reachable ==(distance<detector.outer->g.nodes()));
	setReachable(u,reachable);

	if(u<detector.dist_lits.size()){
		assert(distance>=0);
		assert(distance<detector.outer->g.nodes());
		for(int i = 0;i<detector.dist_lits[u].size();i++){
			int d =  detector.dist_lits[u][i].min_distance;
			Lit l = detector.dist_lits[u][i].l;
			assert(l!=lit_Undef);
			if(d<distance && !polarity){
				lbool assign = detector.outer->value(l);
				if( assign!= l_False ){
					detector.changed.push({~l,u});
				}
			}else if(d>=distance && polarity){
				lbool assign = detector.outer->value(l);
				if( assign!= l_True ){
					detector.changed.push({l,u});
				}
			}
		}
	}*/
}

bool ReachDetector::ChokepointStatus::mustReach(int node){
	Lit l =  detector.reach_lits[node];
	if(l!=lit_Undef){
		return detector.outer->value(l)==l_True;
	}
	return false;
}
bool ReachDetector::ChokepointStatus::operator() (int edge_id){
	return detector.outer->value(detector.outer->edge_list[ edge_id].v)==l_Undef;
}

void ReachDetector::preprocess(){
	//vec<bool> pure;
	//pure.growTo(reach_lits.size());
	//can check if all reach lits appear in only one polarity in the solver constraints; if so, then we can disable either check_positive or check_negative
}

void ReachDetector::buildReachReason(int node,vec<Lit> & conflict){
			//drawFull();
			Reach & d = *positive_path_detector;
			double starttime = rtime(2);
			d.update();

			assert(outer->dbg_reachable(d.getSource(),node));

			assert(d.connected_unchecked(node));
			if(opt_learn_reaches ==0 || opt_learn_reaches==2){
				int u = node;
				int p;
				while(( p = d.previous(u)) != -1){
					Edge & edg = outer->edge_list[d.incomingEdge(u)]; //outer->edges[p][u];
					Var e =edg.v;
					lbool val = outer->value(e);
					assert(outer->value(e)==l_True);
					conflict.push(mkLit(e, true));
					u = p;

				}
			}else{
				//Instead of a complete path, we can learn reach variables, if they exist
				int u = node;
				int p;
				while(( p = d.previous(u)) != -1){
					Edge & edg = outer->edge_list[d.incomingEdge(u)]; //outer->edges[p][u];
					Var e =edg.v;
					lbool val = outer->value(e);
					assert(outer->value(e)==l_True);
					conflict.push(mkLit(e, true));
					u = p;
					if( u< reach_lits.size() && reach_lits[u]!=lit_Undef && outer->value(reach_lits[u])==l_True && outer->level(var(reach_lits[u]))< outer->decisionLevel()){
						//A potential (fixed) problem with the above: reach lit can be false, but have been assigned after r in the trail, messing up clause learning if this is a reason clause...
						//This is avoided by ensuring that L is lower level than the conflict.
						Lit l =reach_lits[u];
						assert(outer->value(l)==l_True);
						conflict.push(~l);
						break;
					}
				}

			}
			outer->num_learnt_paths++;
			outer->learnt_path_clause_length+= (conflict.size()-1);
			double elapsed = rtime(2)-starttime;
			outer->pathtime+=elapsed;
	#ifdef DEBUG_GRAPH
			 assert(outer->dbg_clause(conflict));

	#endif
		}
		void ReachDetector::buildNonReachReason(int node,vec<Lit> & conflict){
			static int it = 0;
			++it;
			if(it==2){
				int a=1;
			}
			int u = node;
			//drawFull( non_reach_detectors[detector]->getSource(),u);
			assert(outer->dbg_notreachable( source,u));
			//assert(!negative_reach_detector->connected_unchecked(node));
			double starttime = rtime(2);
			outer->cutGraph.clearHistory();
			outer->stats_mc_calls++;
			if(opt_conflict_min_cut){
				if(mincutalg!= MinCutAlg::ALG_EDKARP_ADJ){
					//ok, set the weights for each edge in the cut graph.
					//Set edges to infinite weight if they are undef or true, and weight 1 otherwise.
					for(int u = 0;u<outer->cutGraph.nodes();u++){
						for(int j = 0;j<outer->cutGraph.nIncident(u);j++){
							int v = outer->cutGraph.incident(u,j).node;
							Var var = outer->edges[u][v].v;
							/*if(S->value(var)==l_False){
								mc.setCapacity(u,v,1);
							}else{*/
							outer->mc->setCapacity(u,v,0xF0F0F0);
							//}
						}
					}

					//find any edges assigned to false, and set their capacity to 1
					for(int i =0;i<outer->trail.size();i++){
						if(outer->trail[i].isEdge && !outer->trail[i].assign){
							outer->mc->setCapacity(outer->trail[i].from, outer->trail[i].to,1);
						}
					}
				}
				outer->cut.clear();

				int f =outer->mc->minCut(source,node,outer->cut);
				assert(f<0xF0F0F0); assert(f==outer->cut.size());//because edges are only ever infinity or 1
				for(int i = 0;i<outer->cut.size();i++){
					MaxFlow::Edge e = outer->cut[i];

					Lit l = mkLit( outer->edges[e.u][e.v].v,false);
					assert(outer->value(l)==l_False);
					conflict.push(l);
				}
			}else{
				//We could learn an arbitrary (non-infinite) cut here, or just the whole set of false edges
				//or perhaps we can learn the actual 1-uip cut?


					vec<int>& to_visit  = outer->to_visit;
					vec<char>& seen  = outer->seen;

				    to_visit.clear();
				    to_visit.push(node);
				    seen.clear();
					seen.growTo(outer->nNodes());
				    seen[node]=true;

				    do{

				    	assert(to_visit.size());
				    	int u = to_visit.last();
				    	assert(u!=source);
				    	to_visit.pop();
				    	assert(seen[u]);
				    	assert(!outer->dbg_reachable(source,u,false));
				    	//assert(!negative_reach_detector->connected_unsafe(u));
				    	//Ok, then add all its incoming disabled edges to the cut, and visit any unseen, non-disabled incoming.edges()
				    	for(int i = 0;i<outer->inv_adj[u].size();i++){
				    		int v = outer->inv_adj[u][i].v;
				    		int from = outer->inv_adj[u][i].from;
				    		int edge_num =outer->getEdgeID(v);// v-outer->min_edge_var;
				    		if(from==u){
				    			assert(outer->edge_list[edge_num].to == u);
				    			assert(outer->edge_list[edge_num].from == u);
				    			continue;//Self loops are allowed, but just make sure nothing got flipped around...
				    		}
				    		assert(from!=u);
				    		assert(outer->inv_adj[u][i].to==u);
				    		//Note: the variable has to not only be assigned false, but assigned false earlier in the trail than the reach variable...


				    		if(outer->value(v)==l_False){
				    			//note: we know we haven't seen this edge variable before, because we know we haven't visited this node before
				    			//if we are already planning on visiting the from node, then we don't need to include it in the conflict (is this correct?)
				    			//if(!seen[from])
				    				conflict.push(mkLit(v,false));
				    		}else{
				    			assert(from!=source);
				    			//even if it is undef? probably...
				    			if(!seen[from]){
				    				seen[from]=true;
				    				if((opt_learn_reaches ==2 || opt_learn_reaches==3) && from< reach_lits.size() && reach_lits[from]!=lit_Undef && outer->value(reach_lits[from])==l_False  && outer->level(var(reach_lits[from]))< outer->decisionLevel())
				    				{
				    				//The problem with the above: reach lit can be false, but have been assigned after r in the trail, messing up clause learning if this is a reason clause...
				    					Lit r = reach_lits[from];
				    					assert(var(r)<outer->nVars());
				    					assert(outer->value(r)==l_False);
				    					conflict.push(r);
				    				}else
				    					to_visit.push(from);
				    			}
				    		}
				    	}
				    }  while (to_visit.size());




			}


		    if(opt_shrink_theory_conflicts){
		    //visit each edge lit in this initial conflict, and see if unreachability is preserved if we add the edge back in (temporarily)
		    	int i,j=0;

		    	while(cutgraph.nodes()<g.nodes()){
		    		cutgraph.addNode();
		    	}
		    	while(cutgraph.nEdgeIDs()<g.nEdgeIDs()){
		    		//if an edge hasn't been disabled at level 0, add it here.
		    		int edgeID = cutgraph.nEdgeIDs();
		    		Var v = outer->getEdgeVar(edgeID);
		    		Edge & e = outer->edge_list[edgeID];
					cutgraph.addEdge(e.from,e.to,edgeID);
		    		if(outer->value(v)==l_False && outer->level(v)==0){
		    			//permanently disabled edge
		    			cutgraph.disableEdge(edgeID);
		    		}
		    	}
		    	cutgraph.drawFull();
#ifndef NDEBUG
		    	for(int i = 0;i<g.nEdgeIDs();i++){
		    		Var v = outer->getEdgeVar(i);
					if(outer->value(v)==l_False && outer->level(v)==0){

					}else
						assert(cutgraph.edgeEnabled(i));
		    	}
#endif
		    	removed_edges.clear();
		    	for(i = 0;i<conflict.size();i++){
					Lit l = conflict[i];
					if(!sign(l) && outer->isEdgeVar(var(l))){
						int edgeID = outer->getEdgeID(var(l));
						cutgraph.disableEdge(edgeID);
						removed_edges.push(edgeID);
					}
		    	}
		    	cutgraph.drawFull();
		    	for(i = 0;i<conflict.size();i++){
		    		Lit l = conflict[i];
		    		if(!sign(l) && outer->isEdgeVar(var(l))){
		    			//check if the target is still unreachable if we add this lit back in
		    			int edgeID = outer->getEdgeID(var(l));
		    			assert(!antig.edgeEnabled(edgeID));
		    			assert(!cutgraph.edgeEnabled(edgeID));
		    			assert(!cutgraph_reach_detector->connected(node));
		    			cutgraph.enableEdge(edgeID);
		    			if(cutgraph_reach_detector->connected(node)){
		    				cutgraph.disableEdge(edgeID);
		    				conflict[j++]=l;
		    			}else{
		    				//we can drop this edge from the conflict.
		    				stats_shrink_removed++;
		    			}
		    		}else{
		    			conflict[j++]=l;
		    		}

		    	}
		    	conflict.shrink(i-j);
		    	//restore the state of the graph
		    	for(int edgeID: removed_edges){
		    		cutgraph.enableEdge(edgeID);
		    	}

		    	changed.clear();

		    }

		    if(opt_learn_unreachable_component && conflict.size()>1){
		    	//Taking a page from clasp, instead of just learning that node u is unreachable unless one of these edges is flipped,
		    	//we are going to learn that the whole strongly connected component attached to u is unreachable (if that component has more than one node, that is)
		    	assert(conflict[0]==~reach_lits[node]);
		    	std::vector<int> component;
		    	vec<Lit> reach_component;
		    	/*DFSReachability<> d(u,g);

		    	*/
		    	//antig.drawFull();
		    	TarjansSCC<>::getSCC(node,antig,component);
		    	assert(std::count(component.begin(),component.end(),node));
		    	for(int n:component){
		    		if(reach_lits[n]!=lit_Undef){
		    			reach_component.push(reach_lits[n]);
		    		}
		    	}
		    	assert(reach_component.size());
		    	if(reach_component.size()>1){
		    		//create a new literal in the solver


		    		//component must be reachable
		    		bool must_be_reached=false;
		    		for(Lit l:reach_component){
						if(outer->value(l)==l_True && outer->level(var(l))==0){
							must_be_reached=true;
						}
					}
		    		extra_conflict.clear();

		    		if(must_be_reached){

		    			for(int i = 1;i<conflict.size();i++)
		    				extra_conflict.push(conflict[i]);
		    			outer->addClauseSafely(extra_conflict);
		    			stats_learnt_components++;
		    				    		stats_learnt_components_sz+=reach_component.size();
		    		}else{

		    			stats_learnt_components++;
						stats_learnt_components_sz+=reach_component.size();
						Lit component_reachable = mkLit(outer->newVar());
						conflict.copyTo(extra_conflict);
						extra_conflict[0]=~component_reachable;

						outer->addClauseSafely(extra_conflict);

						for(Lit l:reach_component){
							if(outer->value(l)==l_True && outer->level(var(l))==0){
								continue;
							}
							extra_conflict.clear();
							extra_conflict.push(component_reachable);
							extra_conflict.push(~l);
							outer->addClauseSafely(extra_conflict);
						}
		    		}
		    	}else{
		    		//just learn the normal conflict clause
		    	}
		    }

			outer->num_learnt_cuts++;
			outer->learnt_cut_clause_length+= (conflict.size()-1);

			double elapsed = rtime(2)-starttime;
			 outer->mctime+=elapsed;

	#ifdef DEBUG_GRAPH
			 assert(outer->dbg_clause(conflict));
	#endif
		}

		/**
		 * Explain why an edge was forced (to true).
		 * The reason is that _IF_ that edge is false, THEN there is a cut of disabled edges between source and target
		 * So, create the graph that has that edge (temporarily) assigned false, and find a min-cut in it...
		 */
		void ReachDetector::buildForcedEdgeReason(int reach_node, int forced_edge_id,vec<Lit> & conflict){
					static int it = 0;
					++it;

					assert(outer->value(outer->edge_list[forced_edge_id].v)==l_True);
					Lit edgeLit =mkLit( outer->edge_list[forced_edge_id].v,false);

					conflict.push(edgeLit);

					int forced_edge_from = outer->edge_list[forced_edge_id].from;
					int forced_edge_to= outer->edge_list[forced_edge_id].to;


					int u = reach_node;
					//drawFull( non_reach_detectors[detector]->getSource(),u);
					assert(outer->dbg_notreachable( source,u));
					double starttime = rtime(2);
					outer->cutGraph.clearHistory();
					outer->stats_mc_calls++;



					if(opt_conflict_min_cut){
						if(mincutalg!= MinCutAlg::ALG_EDKARP_ADJ){
							//ok, set the weights for each edge in the cut graph.
							//Set edges to infinite weight if they are undef or true, and weight 1 otherwise.
							for(int u = 0;u<outer->cutGraph.nodes();u++){
								for(int j = 0;j<outer->cutGraph.nIncident(u);j++){
									int v = outer->cutGraph.incident(u,j).node;
									Var var = outer->edges[u][v].v;
									/*if(S->value(var)==l_False){
										mc.setCapacity(u,v,1);
									}else{*/
									outer->mc->setCapacity(u,v,0xF0F0F0);
									//}
								}
							}

							//find any edges assigned to false, and set their capacity to 1
							for(int i =0;i<outer->trail.size();i++){
								if(outer->trail[i].isEdge && !outer->trail[i].assign){
									outer->mc->setCapacity(outer->trail[i].from, outer->trail[i].to,1);
								}
							}

							outer->mc->setCapacity(forced_edge_from, forced_edge_to,1);
						}
						outer->cut.clear();

						int f =outer->mc->minCut(source,reach_node,outer->cut);
						assert(f<0xF0F0F0); assert(f==outer->cut.size());//because edges are only ever infinity or 1
						for(int i = 0;i<outer->cut.size();i++){
							MaxFlow::Edge e = outer->cut[i];

							Lit l = mkLit( outer->edges[e.u][e.v].v,false);
							assert(outer->value(l)==l_False);
							conflict.push(l);
						}
					}else{
						//We could learn an arbitrary (non-infinite) cut here, or just the whole set of false edges
						//or perhaps we can learn the actual 1-uip cut?


							vec<int>& to_visit  = outer->to_visit;
							vec<char>& seen  = outer->seen;

						    to_visit.clear();
						    to_visit.push(reach_node);
						    seen.clear();
							seen.growTo(outer->nNodes());
						    seen[reach_node]=true;

						    do{

						    	assert(to_visit.size());
						    	int u = to_visit.last();
						    	assert(u!=source);
						    	to_visit.pop();
						    	assert(seen[u]);
						    	assert(!negative_reach_detector->connected_unsafe(u));
						    	//Ok, then add all its incoming disabled edges to the cut, and visit any unseen, non-disabled incoming.edges()
						    	for(int i = 0;i<outer->inv_adj[u].size();i++){
						    		int v = outer->inv_adj[u][i].v;
						    		int from = outer->inv_adj[u][i].from;
						    		assert(from!=u);
						    		assert(outer->inv_adj[u][i].to==u);
						    		//Note: the variable has to not only be assigned false, but assigned false earlier in the trail than the reach variable...
						    		int edge_num =outer->getEdgeID(v);// v-outer->min_edge_var;

						    		if(edge_num == forced_edge_id || outer->value(v)==l_False){
						    			//note: we know we haven't seen this edge variable before, because we know we haven't visited this node before
						    			//if we are already planning on visiting the from node, then we don't need to include it in the conflict (is this correct?)
						    			//if(!seen[from])
						    				conflict.push(mkLit(v,false));
						    		}else{
						    			assert(from!=source);
						    			//even if it is undef? probably...
						    			if(!seen[from]){
						    				seen[from]=true;
						    				if((opt_learn_reaches ==2 || opt_learn_reaches==3) && from< reach_lits.size() && reach_lits[from]!=lit_Undef && outer->value(reach_lits[from])==l_False  && outer->level(var(reach_lits[from]))< outer->decisionLevel())
						    				{
						    				//The problem with the above: reach lit can be false, but have been assigned after r in the trail, messing up clause learning if this is a reason clause...
						    					Lit r = reach_lits[from];
						    					assert(var(r)<outer->nVars());
						    					assert(outer->value(r)==l_False);
						    					conflict.push(r);
						    				}else
						    					to_visit.push(from);
						    			}
						    		}
						    	}
						    }  while (to_visit.size());


					}

					 outer->num_learnt_cuts++;
					 outer->learnt_cut_clause_length+= (conflict.size()-1);

					double elapsed = rtime(2)-starttime;
					 outer->mctime+=elapsed;

			#ifdef DEBUG_GRAPH
					 assert(outer->dbg_clause(conflict));
			#endif
				}

		void ReachDetector::buildReason(Lit p, vec<Lit> & reason, CRef marker){
				if(marker==reach_marker){
					reason.push(p);
				//	double startpathtime = rtime(2);

					/*Dijkstra & detector = *reach_detectors[d]->positive_dist_detector;
					//the reason is a path from s to p(provided by d)
					//p is the var for a reachability detector in dijkstra, and corresponds to a node
					detector.update();
					Var v = var(p);
					int u =reach_detectors[d]->getNode(v); //reach_detectors[d]->reach_lit_map[v];
					assert(detector.connected(u));
					int w;
					while(( w= detector.previous(u)) > -1){
						Lit l = mkLit( edges[w][u].v,true );
						assert(S->value(l)==l_False);
						reason.push(l);
						u=w;
					}*/
					Var v = var(p);
					int u =getNode(v);
					buildReachReason(u,reason);

		#ifdef DEBUG_GRAPH
				 assert(outer->dbg_clause(reason));

		#endif
					//double elapsed = rtime(2)-startpathtime;
				//	pathtime+=elapsed;
				}else if(marker==non_reach_marker){
					reason.push(p);

					//the reason is a cut separating p from s;
					//We want to find a min-cut in the full graph separating, where activated edges (ie, those still in antig) are weighted infinity, and all others are weighted 1.

					//This is a cut that describes a minimal set of edges which are disabled in the current graph, at least one of which would need to be activated in order for s to reach p
					//assign the mincut edge weights if they aren't already assigned.


					Var v = var(p);
					int t = getNode(v); // v- var(reach_lits[d][0]);
					buildNonReachReason(t,reason);

				}else if (marker==forced_reach_marker){
					Var v = var(p);
					//The forced variable is an EDGE that was forced.
					int forced_edge_id =outer->getEdgeID(v);//v- outer->min_edge_var;
					//The corresponding node that is the reason it was forced
					int reach_node=force_reason[forced_edge_id];
					 buildForcedEdgeReason( reach_node,  forced_edge_id, reason);
				}else{
					assert(false);
				}
		}

		bool ReachDetector::propagate(vec<Lit> & conflict){

			getChanged().clear();

			if(positive_reach_detector && (!opt_detect_pure_theory_lits || unassigned_positives>0)){
				double startdreachtime = rtime(2);
				stats_under_updates++;
				positive_reach_detector->update();
				double reachUpdateElapsed = rtime(2)-startdreachtime;
				//outer->reachupdatetime+=reachUpdateElapsed;
				stats_under_update_time+=rtime(2)-startdreachtime;
			}else{
				//outer->stats_pure_skipped++;
				stats_skipped_under_updates++;
			}

			if(negative_reach_detector && (!opt_detect_pure_theory_lits || unassigned_negatives>0)){
				double startunreachtime = rtime(2);
				stats_over_updates++;
				negative_reach_detector->update();
				double unreachUpdateElapsed = rtime(2)-startunreachtime;
				//outer->unreachupdatetime+=unreachUpdateElapsed;
				stats_over_update_time+=rtime(2)-startunreachtime;
			}else
				stats_skipped_over_updates++;


			if(opt_rnd_shuffle){
				randomShuffle(rnd_seed, changed);
			}

			for(int j = 0;j<getChanged().size();j++){
					Lit l = getChanged()[j].l;
					int u =  getChanged()[j].u;
					bool reach = !sign(l);
					if(outer->value(l)==l_True){
						//do nothing
					}else if(outer->value(l)==l_Undef){
#ifdef DEBUG_GRAPH
						assert(outer->dbg_propgation(l));
#endif
#ifdef DEBUG_SOLVER
						if(S->dbg_solver)
							S->dbg_check_propagation(l);
#endif
						//trail.push(Assignment(false,reach,detectorID,0,var(l)));
						if(reach)
							outer->enqueue(l,reach_marker) ;
						else
							outer->enqueue(l,non_reach_marker) ;

					}else if (outer->value(l)==l_False){
						conflict.push(l);

						if(reach){

						//conflict
						//The reason is a path in g from to s in d
							buildReachReason(u,conflict);
						//add it to s
						//return it as a conflict

						}else{
							//The reason is a cut separating s from t
							buildNonReachReason(u,conflict);

						}
#ifdef DEBUG_GRAPH
						for(int i = 0;i<conflict.size();i++)
									 assert(outer->dbg_value(conflict[i])==l_False);
#endif
#ifdef DEBUG_SOLVER
						if(S->dbg_solver)
							S->dbg_check(conflict);
#endif


						return false;
					}

					if(opt_reach_prop){
						forced_edges.clear();
						chokepoint.collectForcedEdges(forced_edges);
						for(int i = 0;i<forced_edges.size();i++){
							int edge_id = forced_edges[i].edge_id;
							int node = forced_edges[i].node;
							Lit l = mkLit( outer->edge_list[edge_id].v,false);
							if(outer->value(l)==l_Undef){
								force_reason.growTo(edge_id+1);
								force_reason[edge_id]=node;
								outer->enqueue(l,forced_reach_marker);
							}else if(outer->value(l)==l_True){
								//do nothing

							}else{
								//conflict.
								//this actually shouldn't be possible (at this point in the code)
								buildForcedEdgeReason(node,edge_id,conflict);
								return false;
							}
						}

					}

				}

			#ifdef DEBUG_DIJKSTRA
					for(int i = 0;i<reach_lits.size();i++){
						Lit l = reach_lits[i];
						if(l!=lit_Undef){
							int u = getNode(var(l));
							if((positive_reach_detector && (!opt_detect_pure_theory_lits || unassigned_positives>0) && positive_reach_detector->connected_unsafe(u))){
								assert(outer->value(l)==l_True);
								assert(outer->dbg_value(l)==l_True);
							}else if (negative_reach_detector && ((!opt_detect_pure_theory_lits || unassigned_negatives>0) && !negative_reach_detector->connected_unsafe(u))){
								assert(outer->value(l)==l_False);
								assert(outer->dbg_value(l)==l_False);
							}
						}

					}
			#endif
			return true;
		}

bool ReachDetector::checkSatisfied(){
	if(positive_reach_detector && negative_reach_detector){
				for(int j = 0;j< reach_lits.size();j++){
					Lit l = reach_lits[j];
					if(l!=lit_Undef){
						int node =getNode(var(l));

						if(outer->value(l)==l_True){
							if(!positive_reach_detector->connected(node)){
								return false;
							}
						}else if (outer->value(l)==l_False){
							if( negative_reach_detector->connected(node)){
								return false;
							}
						}else{
							if(positive_reach_detector->connected(node)){
								return false;
							}
							if(!negative_reach_detector->connected(node)){
								return false;
							}
						}
					}
				}
	}else{
		Dijkstra<>under(source,g) ;
		Dijkstra<>over(source,antig) ;
		under.update();
		over.update();

		for(int j = 0;j< reach_lits.size();j++){
			Lit l = reach_lits[j];
			if(l!=lit_Undef){
				int node =j;
				if(outer->value(l)==l_True){
					if(!under.connected(node)){
						return false;
					}
				}else if (outer->value(l)==l_False){
					if( over.connected(node)){
						return false;
					}
				}else{
					if(over.connected(node)){
						return false;
					}
					if(!under.connected(node)){
						return false;
					}
				}
			}
		}
	}
	return true;
}

void ReachDetector::dbg_sync_reachability(){
#ifndef NDEBUG
	if(!positive_reach_detector)
		return;
		for(int j = 0;j< reach_lits.size();j++){
						Lit l =reach_lits[j];
						if(l!=lit_Undef){
							int node = getNode(var(l));

							if(positive_reach_detector->connected(node)){
								assert(outer->value(l)==l_True);
							}else if(! negative_reach_detector->connected(node)){
								assert(outer->value(l)==l_False);
							}
						}

					}
#endif
	}


int ReachDetector::OptimalWeightEdgeStatus::operator [] (int edge) const {
	Var v = detector.outer->edge_list[edge].v;
	lbool val = detector.outer->value(v);
	if(val==l_False){
		assert(false);
		return detector.outer->edge_list.size()*2;
	}else if (val==l_True){
		return 0;
	}else{
		return 1;
	}
}
int ReachDetector::OptimalWeightEdgeStatus::size()const{
	return detector.outer->edge_list.size();
}


Lit ReachDetector::decide(){
	if(!negative_reach_detector)
		return lit_Undef;
	auto * over =negative_reach_detector;

	auto * under = positive_reach_detector;


	/*if(opt_decide_graph_chokepoints){

		//we are going to detect chokepoints as follows. For each node that is reachable in over but that is NOT reachable in the under approximation,
		//we will remove its incoming edge. If there are no other paths to that node with that edge removed, then we have found a
		for(int k = 0;k<reach_lits.size();k++){
				Lit l =reach_lits[k];
				if(l==lit_Undef)
					continue;
				int j =getNode(var(l));
				if(outer->value(l)==l_True ){

					assert(over->connected(j));
					if(over->connected(j) && !under->connected(j)){
						//then check to see if there is a chokepoint leading to j, by knocking out the incoming edge and seeing if it is still connected.
						assert(chokepoint_detector->connected(j));//Else, we would already be in conflict
						int edgeID=over->incomingEdge(j);
						assert(edgeID>=0);
						assert(outer->edge_list[edgeID].edgeID==edgeID);
						Var v = outer->edge_list[edgeID].v;
						if(outer->value(v)==l_Undef){

							antig.disableEdge(outer->edge_list[edgeID].from, outer->edge_list[edgeID].to, edgeID);
							bool connected = chokepoint_detector->connected(j);
							bool succeed = antig.rewindHistory(1);
							assert(succeed);
							if(!connected){
								//then edgeID was an unassigned edge that was also a chokepoint leading up to j.
								return mkLit(v,false);
							}
						}
					}

				}
		}
		return lit_Undef;
	}*/

	//we can probably also do something similar, but with cuts, for nodes that are decided to be unreachable.

	//ok, for each node that is assigned reachable, but that is not actually reachable in the under approx, decide an edge on a feasible path

	//this can be obviously more efficient
	//for(int j = 0;j<nNodes();j++){

	while(order_vec.size()<reach_lits.size()){
		order_vec.push(order_vec.size());
	}
	if(opt_rnd_order_graph_decisions){
		randomShuffle(rnd_seed, order_vec);
	}

	if(opt_sort_graph_decisions==0){


	for(int k = 0;k<reach_lits.size();k++){
		Lit l =reach_lits[order_vec[k]];
		if(l==lit_Undef)
			continue;
		int j =getNode(var(l));
		if(outer->value(l)==l_True && opt_decide_graph_pos){
			//if(S->level(var(l))>0)
			//	continue;
			assert(over->connected(j));
			if(over->connected(j) && !under->connected(j)){
				//then lets try to connect this
				static vec<bool> print_path;

				assert(over->connected(j));//Else, we would already be in conflict
				int p =j;
				int last_edge=-1;
				int last=j;
				if(!opt_use_random_path_for_decisions){
					if(opt_use_optimal_path_for_decisions){
						//ok, read back the path from the over to find a candidate edge we can decide
						//find the earliest unconnected node on this path
						opt_path->update();
						 p = j;
						 last = j;
						while(!under->connected(p)){

							last=p;
							assert(p!=source);
							last_edge=opt_path->incomingEdge(p);
							int prev = opt_path->previous(p);
							p = prev;

						}
					}else{
						//ok, read back the path from the over to find a candidate edge we can decide
						//find the earliest unconnected node on this path
						over->update();
						 p = j;
						 last = j;
						while(!under->connected(p)){

							last=p;
							assert(p!=source);
							last_edge=over->incomingEdge(p);
							int prev = over->previous(p);
							p = prev;

						}
					}
				}else{
					//Randomly re-weight the graph sometimes
					if(drand(rnd_seed)<opt_decide_graph_re_rnd){

						for(int i=0;i<outer->edge_list.size() ;i++){
								 double w = drand(rnd_seed);
								/* w-=0.5;
								 w*=w;*/
								 //printf("%f (%f),",w,rnd_seed);
								//rnd_path->setWeight(i,w);
								 rnd_weight[i]=w;
							 }
					}
					rnd_path->update();
					//derive a random path in the graph
					 p = j;
					 last = j;
					 assert(rnd_path->connected(p));
					while(!under->connected(p)){

						last=p;
						assert(p!=source);
						last_edge=rnd_path->incomingEdge(p);
						int prev =rnd_path->previous(p);
						p = prev;
						assert(p>=0);
					}

				}

				if(outer->level(var(l))==0 && opt_print_decision_path){
					print_path.clear();
					print_path.growTo(outer->nNodes());

					if(!opt_use_random_path_for_decisions){
											//ok, read back the path from the over to find a candidate edge we can decide
											//find the earliest unconnected node on this path
						over->update();
						int p = j;

						while(p!=source){
							if(opt_print_decision_path)
								print_path[p]=true;


							assert(p!=source);
							int prev = over->previous(p);
							p = prev;

						}
					}else{


						int p = j;

						while(p!=source){
							if(opt_print_decision_path)
								print_path[p]=true;


							assert(p!=source);
							int prev =rnd_path->previous(p);
							p = prev;

						}
					}

					if(opt_print_decision_path){
						printf("From %d to %d:\n",source,j);
						int width = sqrt(outer->nNodes());


						int v = 0;
						//for (int i = 0;i<w;i++){
						//	for(int j = 0;j<w;j++){
						int lasty= 0;
						for(int n = 0;n<outer->nNodes();n++){
							int x = n%width;
							int y = n/width;
							if(y > lasty)
								printf("\n");

							if(n==j){
								printf("* ");
							}else if (n==source){
								printf("#");
							}else{

								if(print_path[n]){
									printf("+ ");
								}else{
									printf("- ");
								}
							}

							lasty=y;
						}
						printf("\n\n");
					}
				}

				//ok, now pick some edge p->last that will connect p to last;
			/*	assert(!under->connected(last));
				assert(under->connected(p));

				assert(over->connected(last));
				assert(over->connected(p));*/
				assert(last_edge>=0);
				assert(outer->edge_list[last_edge].edgeID==last_edge);
				Var v = outer->edge_list[last_edge].v;
				if(outer->value(v)==l_Undef){
					return mkLit(v,false);
				}else{
					assert(outer->value(v)!=l_True);
				}
			/*	for(int k = 0;k<outer->antig.adjacency[p].size();k++){
					int to = outer->antig.adjacency[p][k].node;
					if (to==last){
						Var v =outer->edge_list[ outer->antig.adjacency[p][k].id].v;
						if(outer->value(v)==l_Undef){
							return mkLit(v,false);
						}else{
							assert(outer->value(v)!=l_True);
						}
					}
				}*/

			}
		}else if(outer->value(l)==l_False && opt_decide_graph_neg){


				//for each negated reachability constraint, we can find a cut through the unassigned edges in the over-approx and disable one of those edges.
				assert(!under->connected(j));
				over->update();
				if(over->connected(j) && ! under->connected(j)){
					//then lets try to disconnect this node from source by walking back along the path in the over approx, and disabling the first unassigned edge we see.
					//(there must be at least one such edge, else the variable would be connected in the under approximation as well - in which case it would already have been propagated.
					int p = j;
					int last = j;
					while(!under->connected(p)){
						last=p;
						assert(p!=source);
						int prev = over->previous(p);
						int incoming_edge = over->incomingEdge(p);
						Var v = outer->edge_list[incoming_edge].v;
						if(outer->value(v)==l_Undef){
							return mkLit(v,true);
						}else{
							assert(outer->value(v)!=l_False);
						}
						p = prev;

					}


				}

		}

	}
	}else{

		int shortest_incomplete_path=-1;
		int edgeID_to_assign=-1;
		for(int k = 0;k<reach_lits.size();k++){
				Lit l =reach_lits[order_vec[k]];
				if(l==lit_Undef)
					continue;
				int j =getNode(var(l));
				if(outer->value(l)==l_True && opt_decide_graph_pos){
					//if(S->level(var(l))>0)
					//	continue;
					assert(over->connected(j));
					if(over->connected(j) && !under->connected(j)){
						//then lets try to connect this
						assert(over->connected(j));//Else, we would already be in conflict
						int p =j;
						int last_edge=-1;
						int last=j;
						//ok, read back the path from the over to find a candidate edge we can decide
						//find the earliest unconnected node on this path
						int dist=0;
						over->update();
						 p = j;
						 last = j;
						while(!under->connected(p)){
							dist++;
							last=p;
							assert(p!=source);
							last_edge=over->incomingEdge(p);
							assert(outer->value( outer->edge_list[last_edge].v)==l_Undef);
							int prev = over->previous(p);
							p = prev;

						}
						assert(dist>0);
						if(opt_sort_graph_decisions==1){
							if(shortest_incomplete_path<0 || dist<shortest_incomplete_path){
								shortest_incomplete_path=dist;
								edgeID_to_assign=last_edge;
							}
						}else{
							if(dist>shortest_incomplete_path){
								shortest_incomplete_path=dist;
								edgeID_to_assign=last_edge;
							}
						}
					}
				}
		}

		if(edgeID_to_assign>=0){
			assert(outer->edge_list[edgeID_to_assign].edgeID==edgeID_to_assign);
			Var v = outer->edge_list[edgeID_to_assign].v;
			if(outer->value(v)==l_Undef){
				return mkLit(v,false);
			}else{
				assert(outer->value(v)!=l_True);
			}
		}

	}
	return lit_Undef;
};


