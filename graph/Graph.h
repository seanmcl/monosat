/*
 * Graph.h
 *
 *  Created on: 2013-04-13
 *      Author: sam
 */

#ifndef GRAPH_H_
#define GRAPH_H_

#include "mtl/Vec.h"
#include "mtl/Heap.h"
#include "mtl/Alg.h"
#include "dgl/graph/DynamicGraph.h"
#include "core/SolverTypes.h"
#include "core/Theory.h"

namespace Minisat {

class GraphTheory: public Theory{

public:

    virtual ~GraphTheory(){};
    virtual int getGraphID()=0;
	virtual int newNode()=0;
	virtual void newNodes(int n)=0;
	virtual int nNodes()=0;
	virtual bool isNode(int n)=0;
	virtual Lit newEdge(int from,int to, Var v, int weight=1)=0;
	virtual int getWeight(int edgeID)=0;
	virtual void reachesAny(int from, Var firstVar,int within_steps)=0;
	virtual void reachesAny(int from, vec<Lit> & properties_out,int within_steps)=0;
	virtual void reaches(int from,int to, Var reach_var,int within_steps=-1)=0;
	virtual void connects(int from,int to, Var reach_var,int within_steps=-1)=0;
	virtual void printStats(int detailLevel)=0;
	virtual void preprocess(){

	}
	virtual void implementConstraints()=0;

	//v will be true if the minimum weight is <= the specified value
	virtual void minimumSpanningTree(Var v, int minimum_weight)=0;
	virtual void edgeInMinimumSpanningTree(Var edgeVar, Var var)=0;
	virtual void maxFlow(int from, int to, int max_flow, Var v)=0;
	virtual void minConnectedComponents(int min_components, Var v)=0;
	virtual void addSteinerTree(const vec<std::pair<int, Var> > & terminals, int steinerTreeID)=0;
	virtual void addSteinerWeightConstraint(int steinerTreeID, int weight, Var outerVar)=0;
};

class Graph{
public:
	static const int Undef_Node =-1;


public:

    virtual ~Graph(){};


public:
	virtual int newNode()=0;
	virtual int nNodes()=0;
	virtual bool isNode(int n)=0;
	virtual Lit newEdge(int from,int to)=0;
	virtual void reachesAny(int from, Var firstVar,int within_steps)=0;
	virtual void reachesAny(int from, vec<Lit> & properties_out,int within_steps)=0;

	virtual Lit decideTheory()=0;
};
};

#endif /* GRAPH_H_ */