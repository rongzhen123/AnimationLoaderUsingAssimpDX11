#include "Entity.h"
#include "skinnedmesh.h"

class AnimateEntity:Entity
{
	AnimateEntity(const String& name, const SkinnedMesh* mesh);
	~AnimateEntity();

	const SkinnedMesh* skinnedmesh;

	String mCurrentAnimation;

	void Render(const String& animationname);
};