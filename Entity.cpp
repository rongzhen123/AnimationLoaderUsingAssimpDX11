#include "Entity.h"

Entity::Entity(const String & name, const EntityType & type):
	mName(name),
	mEntityType(type),
	mVisible(true),
	mUpperDistance(1000),
	mSquraredUpperDistance(1000000),
	mBeyondFarDistance(true),
	mWorldAABB(),
	mWorldBoundingSphere(),
	mFullBoudingBox()
{

}

Entity::~Entity()
{

}
