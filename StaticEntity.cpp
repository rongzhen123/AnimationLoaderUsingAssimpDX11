#include "StaticEntity.h"

StaticEntity::StaticEntity(const String & name, const Mesh * mesh):mesh(mesh),Entity(name,EntityType::Static_Entity)
{

}

StaticEntity::~StaticEntity()
{
	mesh = NULL;
}
