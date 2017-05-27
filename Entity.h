#ifndef __Entity_H__
#define __Entity_H__

#include "Prerequisites.h"
#include "OgreAxisAlignedBox.h"
#include "OgreSphere.h"
#include "OgreQuaternion.h"
#include "OgreVector3.h"

class Entity
{
public:
	String mName;
	//OctreeSceneManager* mManager;
	OctreeSceneNode* mParentNode;
	bool mVisible;
	Real mUpperDistance;
	Real mSquraredUpperDistance;
	//Real mMinPixelSize;
	//Hidden because of distance?
	bool mBeyondFarDistance;

	mutable AxisAlignedBox mWorldAABB;
	mutable Sphere mWorldBoundingSphere;

	EntityType mEntityType;
	//typedef std::map<String, Entity*> childList;

	mutable AxisAlignedBox mFullBoudingBox;

	Entity(const String& name,const EntityType& type);
	~Entity();

};


#endif