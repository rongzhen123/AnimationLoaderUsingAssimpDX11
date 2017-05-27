#include "AnimateEntity.h"


AnimateEntity::AnimateEntity(const String & name, const SkinnedMesh * mesh) :skinnedmesh(mesh), Entity(name, EntityType::Animate_Entity)
{

}


AnimateEntity::~AnimateEntity()
{
	
}

void AnimateEntity::Render(const String & animationname)
{
	if (skinnedmesh != NULL)
	{

	}
}
