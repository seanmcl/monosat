/*
 * Shape.h
 *
 *  Created on: 2014-02-23
 *      Author: sam
 */

#ifndef SHAPE_H_
#define SHAPE_H_
#include "mtl/Vec.h"
#include "GeometryTypes.h"

/**
 * A concrete shape
 */
template<unsigned int D,class T=double>
class Shape{

public:
	Shape(){
	}
	virtual ~Shape(){};
	int dimension(){
		return D;
	}

	virtual bool contains(Point<D,T> & point)=0;
	virtual bool intersects(Shape<D,T> & s)=0;


};



#endif /* SHAPE_H_ */
