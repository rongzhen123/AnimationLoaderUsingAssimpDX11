#include "OctreeSceneNode.h"

OctreeSceneNode::QueuedUpdates OctreeSceneNode::msQueuedUpdates;

OctreeSceneNode::OctreeSceneNode(const String& name)
	:mParent(0),
	mNeedParentUpdate(false),
	mNeedChildUpdate(false),
	mParentNotified(false),
	mQueuedForUpdate(false),
	mName(name),
	mOrientation(Quaternion::IDENTITY),
	mPosition(Vector3::ZERO),
	mScale(Vector3::UNIT_SCALE),
	mInheritOrientation(true),
	mInheritScale(true),
	mDerivedOrientation(Quaternion::IDENTITY),
	mDerivedPosition(Vector3::ZERO),
	mDerivedScale(Vector3::UNIT_SCALE),
	mCachedTransformOutOfDate(true)
	{
		needUpdate();
	}

OctreeSceneNode::~OctreeSceneNode()
{
	
	removeAllChildren();
	
	if (mParent)
		mParent->removeChild(this);

	if (mQueuedForUpdate)
	{
		//Erase from queued updates
		QueuedUpdates::iterator it = std::find(msQueuedUpdates.begin(), msQueuedUpdates.end(), this);
		assert(it!= msQueuedUpdates.end());
		if (it != msQueuedUpdates.end())
		{
			//Optimised algorithm to erase an element from unordered vector.
			*it = msQueuedUpdates.back();
			msQueuedUpdates.pop_back();
		}
	}
}

void OctreeSceneNode::attachEntity(Entity * entity)
{
}

void OctreeSceneNode::setOrientation(const Quaternion & q)
{
	if (q.isNaN())
	{
		String tip;
		tip.append("Invalid orientation supplied as parameter");
		MessageBoxA(NULL, tip.c_str(), "ERROR", MB_ICONERROR);
	}
	mOrientation = q;
	mOrientation.normalise();
	needUpdate();
}

void OctreeSceneNode::setOrientation(Real w, Real x, Real y, Real z)
{
	setOrientation(Quaternion(w, x, y, z));
}

void OctreeSceneNode::resetOrientation()
{
	mOrientation = Quaternion::IDENTITY;
	needUpdate();
}

void OctreeSceneNode::setPosition(const Vector3 & pos)
{
	assert(!pos.isNaN() && "Invalid vector supplied as parameter");
	mPosition = pos;
	needUpdate();
}

void OctreeSceneNode::setPosition(Real x, Real y, Real z)
{
	Vector3 v(x,y,z);
	setPosition(v);
}

void OctreeSceneNode::setScale(const Vector3 & scale)
{
	assert(!scale.isNaN() && "Invalid vector supplied as parameter");
	mScale = scale;
	needUpdate();
}

void OctreeSceneNode::setScale(Real x, Real y, Real z)
{
	setScale(Vector3(x,y,z));
}

void OctreeSceneNode::setInheritOrientation(bool inherit)
{
	mInheritOrientation = inherit;
	needUpdate();
}

void OctreeSceneNode::setInheritScale(bool inherit)
{
	mInheritScale = inherit;
	needUpdate();
}

void OctreeSceneNode::scale(const Vector3 & scale)
{
	mScale = mScale * scale;
	needUpdate();
}

void OctreeSceneNode::scale(Real x, Real y, Real z)
{
	mScale.x *= x;
	mScale.y *= y;
	mScale.z *= z;
	needUpdate();
}

void OctreeSceneNode::translate(const Vector3 & d, TransformSpace relativeTo)
{
	switch (relativeTo)
	{
	case TS_LOCAL:
		//position is relative to parent so transform downwards
		mPosition += mOrientation *d;
		break;
	case TS_WORLD:
		//position is relative to parent so transform upwards
		if (mParent)
		{
			mPosition += mParent->convertWorldToLocalDirection(d, true);
		}
		else
		{
			mPosition += d;
		}
		break;
	case TS_PARENT:
		mPosition += d;
		break;
	}
	needUpdate();
}

void OctreeSceneNode::translate(Real x, Real y, Real z, TransformSpace relativeTo)
{
	Vector3 v(x,y,z);
	translate(v,relativeTo);
}

void OctreeSceneNode::translate(const Matrix3 & axes, const Vector3 & move, TransformSpace relativeTo)
{
	Vector3 derived = axes * move;
	translate(derived,relativeTo);
}

void OctreeSceneNode::translate(const Matrix3 & axes, Real x, Real y, Real z, TransformSpace relativeTo)
{
	Vector3 d(x,y,z);
	translate(axes,d,relativeTo);
}

const Vector3 & OctreeSceneNode::_getDerivedScale() const
{
	// TODO: 在此处插入 return 语句
	if (mNeedParentUpdate)
	{
		_updateFromParent();
	}
	return mDerivedScale;
}

const Matrix4& OctreeSceneNode::_getFullTransform() const
{
	if (mCachedTransformOutOfDate)
	{
		//use derived values
		mCachedTransform.makeTransform(_getDerivedPosition(),_getDerivedScale(),_getDerivedOrientation());
		mCachedTransformOutOfDate = false;
	}
	return mCachedTransform;
}
void OctreeSceneNode::_update(bool updateChildren, bool parentHasChanged)
{
	//always clear information about parent notification
	mParentNotified = false;
	//see if we should process everyone
	if (mNeedParentUpdate || parentHasChanged)
	{
		//update transform from parent
		_updateFromParent();
	}
	if (updateChildren)
	{
		if (mNeedChildUpdate || parentHasChanged)
		{
			ChildNodeMap::iterator it, itend;
			itend = mChildren.end();
			for (it = mChildren.begin(); it != itend; ++it)
			{
				OctreeSceneNode* child = it->second;
				child->_update(true,true);
			}
		}
		else
		{
			//Just update selected children
			ChildUpdateSet::iterator it, itend;
			itend = mChildrenToUpdate.end();
			for (it = mChildrenToUpdate.begin(); it != itend; ++it)
			{
				OctreeSceneNode* child = *it;
				child->_update(true,false);
			}
		}

		mChildrenToUpdate.clear();
		mNeedChildUpdate = false;
	}
}

Vector3 OctreeSceneNode::convertWorldToLocalPosition(const Vector3 & worldPos)
{
	if (mNeedParentUpdate)
	{
		_updateFromParent();
	}

	return mDerivedOrientation.Inverse() * (worldPos - mDerivedPosition) / mDerivedScale;
}

Vector3 OctreeSceneNode::convertLocalToWorldPosition(const Vector3 & localPos)
{
	if (mNeedParentUpdate)
	{
		_updateFromParent();
	}
	return _getFullTransform().transformAffine(localPos);
}

Vector3 OctreeSceneNode::convertWorldToLocalDirection(const Vector3 & worldDir, bool useScale)
{
	if (mNeedParentUpdate)
	{
		_updateFromParent();
	}
	return useScale? mDerivedOrientation.Inverse() * worldDir / mDerivedScale : 
					 mDerivedOrientation.Inverse() * worldDir;
}

Vector3 OctreeSceneNode::convertLocalToWorldDirection(const Vector3 & localDir, bool useScale)
{
	if (mNeedParentUpdate)
	{
		_updateFromParent();
	}
	return useScale ? _getFullTransform().transformDirectionAffine(localDir) : mDerivedOrientation * localDir;
}

Quaternion OctreeSceneNode::convertWorldToLocalOrientation(const Quaternion & worldOrientation)
{
	if (mNeedParentUpdate)
	{
		_updateFromParent();
	}
	return mDerivedOrientation.Inverse() * worldOrientation;
}

Quaternion OctreeSceneNode::convertLocalToWorldOrientation(const Quaternion & localOrientation)
{
	if (mNeedParentUpdate)
	{
		_updateFromParent();
	}
	return mDerivedOrientation * localOrientation;
}

void OctreeSceneNode::needUpdate(bool forceParentUpdate)
{
	mNeedParentUpdate = true;
	mNeedChildUpdate = true;
	mCachedTransformOutOfDate = true;

	//Make sure we're not root and parent hasn't been notified before
	if (mParent && (!mParentNotified || forceParentUpdate))
	{
		mParent->requestUpdate(this,forceParentUpdate);
		mParentNotified = true;
	}
	//all children will be updated
	mChildrenToUpdate.clear();
}

void OctreeSceneNode::requestUpdate(OctreeSceneNode * child, bool forceParentUpdate)
{
	//If we're already going to update everything this doesn't matter
	if (mNeedChildUpdate)
	{
		return;
	}
	mChildrenToUpdate.insert(child);
	//Request selective update of me,if we didn't do it before
	if (mParent && (!mParentNotified || forceParentUpdate))
	{
		mParent->requestUpdate(this,forceParentUpdate);
		mParentNotified = true;
	}
}

void OctreeSceneNode::cancelUpdate(OctreeSceneNode * child)
{
	mChildrenToUpdate.erase(child);
	//Propagate this up if we are done
	if (mChildrenToUpdate.empty() && mParent && !mNeedChildUpdate)
	{
		mParent->cancelUpdate(this);
		mParentNotified = false;
	}
}

void OctreeSceneNode::queueNeedUpdate(OctreeSceneNode * n)
{
	//Don't queue the node more than once
	if (!n->mQueuedForUpdate)
	{
		n->mQueuedForUpdate = true;
		msQueuedUpdates.push_back(n);
	}
}

void OctreeSceneNode::processQueuedUpdates()
{
	for (QueuedUpdates::iterator i = msQueuedUpdates.begin(); i != msQueuedUpdates.end(); ++i)
	{
		//Update,and force parent update since chances are we've ended up with some mixed state in there due to re-entrancy
		OctreeSceneNode* n = *i;
		n->mQueuedForUpdate = false;
		n->needUpdate(true);
	}
	msQueuedUpdates.clear();
}

void OctreeSceneNode::setParent(OctreeSceneNode * parent)
{
	mParent = parent;
	//Request update from parent
	mParentNotified = false;
	needUpdate();
}

void OctreeSceneNode::_updateFromParent(void) const
{
	updateFromParentImpl();
}

void OctreeSceneNode::updateFromParentImpl(void) const
{
	mCachedTransformOutOfDate = true;
	if (mParent)
	{
		//Update orientation
		const Quaternion& parentOrientation = mParent->_getDerivedOrientation();
		if (mInheritOrientation)
		{
			//Combine orientation with that of parent
			mDerivedOrientation = parentOrientation * mOrientation;
		}
		else
		{
			//No inheritance
			mDerivedOrientation = mOrientation;
		}
		//Update scale
		const Vector3& parentScale = mParent->_getDerivedScale();
		if (mInheritScale)
		{
			mDerivedScale = parentScale * mScale;
		}
		else
		{
			mDerivedScale = mScale;
		}

		//Change position vector based on parent's orientation & scale
		mDerivedPosition = parentOrientation * (parentScale * mPosition);

		//Add altered position vector to parents
		mDerivedPosition += mParent->_getDerivedPosition();
	}
	else
	{
		//Root node,no parent
		mDerivedOrientation = mOrientation;
		mDerivedPosition = mPosition;
		mDerivedScale = mScale;
	}
	mNeedParentUpdate = false;
}

OctreeSceneNode * OctreeSceneNode::createChildImpl(const String & name)
{
	return nullptr;
}

void OctreeSceneNode::roll(const Radian & angle, TransformSpace relativeTo)
{
	rotate(Vector3::UNIT_Z,angle,relativeTo);
}

void OctreeSceneNode::pitch(const Radian & angle, TransformSpace relativeTo)
{
	rotate(Vector3::UNIT_X,angle,relativeTo);
}

void OctreeSceneNode::yaw(const Radian & angle, TransformSpace relativeTo)
{
	rotate(Vector3::UNIT_Y,angle,relativeTo);
}

void OctreeSceneNode::rotate(const Vector3 & axis, const Radian & angle, TransformSpace relativeTo)
{
	Quaternion q;
	q.FromAngleAxis(angle,axis);
	rotate(q, relativeTo);
}

void OctreeSceneNode::rotate(const Quaternion & q, TransformSpace relativeTo)
{
	switch (relativeTo)
	{
	case TS_PARENT:
		//Rotations are normally relative to local axes,trnaform up
		mOrientation = q * mOrientation;
		break;
	case TS_WORLD:
		mOrientation = mOrientation * _getDerivedOrientation() * q * _getDerivedOrientation();
		break;
	case TS_LOCAL:
		mOrientation = mOrientation * q;
		break;
	}

	//Normalize quaternion to avoid drift
	mOrientation.normalise();
	needUpdate();
}

Matrix3 OctreeSceneNode::getLocalAxes() const
{
	Vector3 axisX = Vector3::UNIT_X;
	Vector3 axisY = Vector3::UNIT_Y;
	Vector3 axisZ = Vector3::UNIT_Z;

	axisX = mOrientation * axisX;
	axisY = mOrientation * axisY;
	axisZ = mOrientation * axisZ;

	return Matrix3(axisX.x,axisY.x,axisZ.x,
					axisX.y,axisY.y,axisZ.y,
					axisX.z,axisY.z,axisZ.z);
}
/*
OctreeSceneNode * OctreeSceneNode::createChild(const Vector3 & translate, const Quaternion & rotate)
{
	OctreeSceneNode* newNode = createChildImpl();
	newNode->setPosition(translate);
	newNode->setOrientation(rotate);
	this->addChild(newNode);
	
	return newNode;
}*/

OctreeSceneNode * OctreeSceneNode::createChild(const String & name, const Vector3 & translate, const Quaternion & rotate)
{
	OctreeSceneNode* newNode = createChildImpl(name);
	newNode->setPosition(translate);
	newNode->setOrientation(rotate);
	this->addChild(newNode);

	return newNode;
}

void OctreeSceneNode::addChild(OctreeSceneNode * child)
{
	if (child->mParent)
	{
		String tip;
		tip.append("Node " + child->getName() + "already was a child of" + child->mParent->getName() + "OctreeSceneNode::addChild");
		MessageBoxA(NULL, tip.c_str(), "ERROR", MB_ICONERROR);
		return;
	}
	typedef std::pair<String, OctreeSceneNode*> nodepair;
	mChildren.insert(nodepair(child->getName(),child));
	child->setParent(this);
}

OctreeSceneNode * OctreeSceneNode::getChild(unsigned short index) const
{
	if (index < mChildren.size())
	{
		ChildNodeMap::const_iterator i = mChildren.begin();
		while (index--) ++i;
		return i->second;
	}
	else
	return nullptr;
}

OctreeSceneNode * OctreeSceneNode::getChild(const String & name) const
{
	ChildNodeMap::const_iterator i = mChildren.find(name);
	if (i == mChildren.end())
	{
		String tip;
		tip.append("Child node " + name + "does not exist.OctreeSceneNode::getChild");
		MessageBoxA(NULL, tip.c_str(), "ERROR", MB_ICONERROR);
		return NULL;
	}
	return i->second;
}

OctreeSceneNode * OctreeSceneNode::removeChild(unsigned short index)
{
	if (index < mChildren.size())
	{
		ChildNodeMap::iterator i = mChildren.begin();
		while (index--) ++i;
		OctreeSceneNode* ret = i->second;

		//cancel any pending update
		cancelUpdate(ret);

		mChildren.erase(i);
		ret->setParent(NULL);
		return ret;
	}
	else
	{
		String tip;
		tip.append("Child index out of bounds OctreeSceneNode::getChild");
		MessageBoxA(NULL, tip.c_str(), "ERROR", MB_ICONERROR);
	}
	return nullptr;
}

OctreeSceneNode * OctreeSceneNode::removeChild(OctreeSceneNode * child)
{
	if (child)
	{
		ChildNodeMap::iterator i = mChildren.find(child->getName());
		//ensure it's our child
		if (i != mChildren.end() && i->second == child)
		{
			//cancel any pending update
			cancelUpdate(child);

			mChildren.erase(i);
			child->setParent(NULL);
		}
	}
	return child;
}

OctreeSceneNode * OctreeSceneNode::removeChild(const String & name)
{
	ChildNodeMap::iterator i = mChildren.find(name);
	if (i == mChildren.end())
	{
		String tip;
		tip.append("Child node named" + name + "does not exist.OctreeSceneNode::removeChild");
		MessageBoxA(NULL, tip.c_str(), "ERROR", MB_ICONERROR);
		return NULL;
	}
	OctreeSceneNode* ret = i->second;
	//Cancel any pending update
	cancelUpdate(ret);

	mChildren.erase(i);
	ret->setParent(NULL);

	return ret;
}

void OctreeSceneNode::removeAllChildren()
{
	ChildNodeMap::iterator i, iend;
	iend = mChildren.end();
	for (i = mChildren.begin(); i != iend; ++i)
	{
		i->second->setParent(0);
	}
	mChildren.clear();
	mChildrenToUpdate.clear();
}

void OctreeSceneNode::_setDerivedPosition(const Vector3 & pos)
{
	//find where the node would end up in parent's local space
	if (mParent)
		setPosition(mParent->convertWorldToLocalPosition(pos));
}

void OctreeSceneNode::_setDerivedOrientation(const Quaternion & q)
{
	//find where the node would end up in parent's local space
	if (mParent)
		setOrientation(mParent->convertWorldToLocalOrientation(q));
}

const Quaternion & OctreeSceneNode::_getDerivedOrientation() const
{
	if (mNeedParentUpdate)
	{
		_updateFromParent();
	}
	return mDerivedOrientation;
}

const Vector3 & OctreeSceneNode::_getDerivedPosition() const
{
	if (mNeedParentUpdate)
	{
		_updateFromParent();
	}
	return mDerivedPosition;
}
