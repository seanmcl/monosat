/*
 * DistanceDetector.h
 *
 *  Created on: 2014-01-08
 *      Author: sam
 */

#ifndef CONNECTED_COMPONENTS_DETECTOR_H_
#define CONNECTED_COMPONENTS_DETECTOR_H_
#include "utils/System.h"

#include "Graph.h"
#include "ConnectedComponents.h"

#include "core/SolverTypes.h"
#include "mtl/Map.h"
#include "DisjointSetConnectedComponents.h"
#include "DisjointSets.h"
#include "utils/System.h"
#include "Detector.h"
namespace Minisat{
class GraphTheorySolver;
class ConnectedComponentsDetector:public Detector{
public:
		GraphTheorySolver * outer;
		//int within;
		DynamicGraph<PositiveEdgeStatus> & g;
		DynamicGraph<NegativeEdgeStatus> & antig;

		double rnd_seed;
		CRef reach_marker;
		CRef non_reach_marker;
		CRef forced_reach_marker;
		ConnectedComponents * positive_reach_detector;
		ConnectedComponents * negative_reach_detector;

		//Reach *  positive_path_detector;

		//vec<Lit>  reach_lits;

		Map<float,int> weight_lit_map;
		vec<int> force_reason;

		struct ConnectedComponentsWeightLit{
			Lit l;
			int min_components;
			ConnectedComponentsWeightLit():l(lit_Undef),min_components(-1){}
		};
		vec<ConnectedComponentsWeightLit>  connected_components_lits;
		struct ConnectedComponentsEdgeLit{
			Lit l;
			int edgeID;
			ConnectedComponentsEdgeLit():l(lit_Undef),edgeID(-1){}
		};
		vec<ConnectedComponentsEdgeLit>  tree_edge_lits;
		struct ChangedWeight{
			Lit l;
			int min_components;
		};
		vec<ChangedWeight> changed_weights;

		struct ChangedEdge{
			Lit l;
			int edgeID;
		};
		vec<ChangedEdge> changed_edges;

		vec<Var> tmp_nodes;
		vec<bool> seen;
		vec<bool> black;
		vec<int> ancestors;

		vec<Lit> tmp_conflict;
		vec<int> visit;
		DisjointSets sets;

		struct ConnectedComponentsStatus{
			ConnectedComponentsDetector & detector;
			bool polarity;

			void setComponents(int components);

			ConnectedComponentsStatus(ConnectedComponentsDetector & _outer, bool _polarity):detector(_outer), polarity(_polarity){}
		};
		ConnectedComponentsStatus *positiveReachStatus;
		ConnectedComponentsStatus *negativeReachStatus;



		bool propagate(vec<Assignment> & trail,vec<Lit> & conflict);
		void buildConnectedReason(vec<Lit> & conflict);
		void buildDisconnectedReason(vec<Lit> & conflict);
		void buildMinComponentsReason(int min_components,vec<Lit> & conflict);
		void buildNotMinComponentsReason(int min_components,vec<Lit> & conflict);
		//void buildForcedMinWeightReason(int reach_node, int forced_edge_id,vec<Lit> & conflict);
		void buildReason(Lit p, vec<Lit> & reason, CRef marker);
		bool checkSatisfied();
		Lit decide();
		void addTreeEdgeLit(int edge_id, Var reach_var);
		void addConnectedComponentsLit(Var weight_var,int min_components);

		ConnectedComponentsDetector(int _detectorID, GraphTheorySolver * _outer, DynamicGraph<PositiveEdgeStatus> &_g, DynamicGraph<NegativeEdgeStatus> &_antig,   double seed=1);//:Detector(_detectorID),outer(_outer),within(-1),source(_source),rnd_seed(seed),positive_reach_detector(NULL),negative_reach_detector(NULL),positive_path_detector(NULL),positiveReachStatus(NULL),negativeReachStatus(NULL){}
		virtual ~ConnectedComponentsDetector(){

		}

};
};
#endif /* DistanceDetector_H_ */
