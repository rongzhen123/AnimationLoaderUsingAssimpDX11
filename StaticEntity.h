#ifndef StaticEntity_H__
#define StaticEntity_H__

#include "Entity.h"
#include "mesh.h"
class StaticEntity:public Entity
{
public:
	StaticEntity(const String& name, const Mesh* mesh);
	~StaticEntity();

	const Mesh* mesh;

};

#endif