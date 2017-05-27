#ifndef _Node_H__
#define _Node_H__

#include "Prerequisites.h"
#include "OgreMatrix4.h"
#include "OgreAxisAlignedBox.h"

class OctreeSceneNode
{
public:
	enum TransformSpace
	{
		//Transfrom is relative to the local space
		TS_LOCAL,
		//Transform is relative to the space of the parent node
		TS_PARENT,
		//Transform is relative to world space
		TS_WORLD
	};
	typedef std::hash_map<String, OctreeSceneNode*> ChildNodeMap;
	typedef std::hash_map<String, Entity*> EntityMap;
protected:
	//OctreeSceneManager* mCreator;
	//Pointer to parent node
	OctreeSceneNode* mParent;
	//Collection of pointers to direct children;hashmap for efficiency
	ChildNodeMap mChildren;

	EntityMap mAttachedEntities;

	AxisAlignedBox mWorldAABB;

	//List of children which need updating,used if self is not out of date but children are
	typedef std::set<OctreeSceneNode*> ChildUpdateSet;
	ChildUpdateSet mChildrenToUpdate;
	//Flag to indicate own transform from parent is out of date
	mutable bool mNeedParentUpdate;
	//Flag indicating that all children need to be updated
	bool mNeedChildUpdate;
	//Flag indicating that parent has been notified about update request
	bool mParentNotified;
	//Flag indicating that the node has been queued for update
	bool mQueuedForUpdate;

	String mName;
	
	//Stores the orientatin of the node relative to it's parent
	Quaternion mOrientation;
	//Stores the position/translation of the node relative to its parent
	Vector3 mPosition;
	//Stores the scaling factor applied to this node
	Vector3 mScale;
	//Stores whether this node inherits orientation from it's parent
	bool mInheritOrientation;
	//Stores whether this node inherits scale from it's parent
	bool mInheritScale;

	void setParent(OctreeSceneNode* parent);

	//Cached combined orientation.this member is the orientation derived by combing the local transformations and those of its
	//parent.This is updated when _updateFromParent is called by the SceneManager or the nodes parent.
	mutable Quaternion mDerivedOrientation;
	//Cached combined position
	mutable Vector3 mDerivedPosition;
	//Cached combined scale
	mutable Vector3 mDerivedScale;

	//Cached derived transform as 4x4 matrix
	mutable Matrix4 mCachedTransform;
	mutable bool mCachedTransformOutOfDate;

	typedef std::vector<OctreeSceneNode*> QueuedUpdates;
	static QueuedUpdates msQueuedUpdates;

	/*Triggers the node to update it's combined transforms.
	This method is called internally by engine to ask the node to update it's complete transformation based on it's parents 
	derived transform
	*/
	void _updateFromParent(void)const;
	/*Class-specific implementation of _updateFromParent
	Splitting the implementation of the update away from the update call itself allows the detail to be overriden without 
	disrupting the general sequence of upateFromParent
	*/

	 void updateFromParentImpl(void) const;

	/*Internal method for creating a new child node -- must be overriden per subclass*/
	//virtual OctreeSceneNode* createChildImpl();
	virtual OctreeSceneNode* createChildImpl(const String& name);



public:
	OctreeSceneNode(const String& name);
	~OctreeSceneNode();

	void attachEntity(Entity* entity);
	/*unsigned short numAttachedEntities() const;
	Entity* getAttachedEntity(unsigned short index);
	Entity* getAttachedEntity(const String& name);
	Entity* detachEntity(unsigned short index);
	void detachEntity(Entity* entity);
	Entity* detachEntity(const String& name);
	void detachAllEntities();
	void _updateBounds();
//	void _findVisibleObjects(Camera* cam,);*/


	const String& getName() const { return mName; }
	OctreeSceneNode* getParent() const { return mParent; }
	//Returns a quaternion representing the ndoes orientation
	const Quaternion & getOrientation() const { return mOrientation; }
	/*Sets the orientation of this node via a quaternion.
	Note that roations are oriented around the node's origin.*/
	void setOrientation(const Quaternion& q);
	void setOrientation(Real w,Real x,Real y,Real z);
	/*Resets the nodes orientation(local axes as world axes,no rotation)*/
	void resetOrientation();
	/*Sets the position of the node relative to it's parent*/
	void setPosition(const Vector3& pos);
	void setPosition(Real x,Real y,Real z);
	/*Gets the position of the node relative to it's parent*/
	const Vector3& getPosition() const { return mPosition; }
	/*Sets the scaling factor applied to this node*/
	void setScale(const Vector3& scale);
	void setScale(Real x,Real y,Real z);
	/*Gets the scaling factor of this node*/
	const Vector3& getScale() const { return mScale; }
	/*Tells the node whether it should inherit orientation from it's parent node.*/
	void setInheritOrientation(bool inherit);
	bool getInheritOrientation(void) const { return mInheritOrientation; }
	void setInheritScale(bool inherit);
	bool getInheritScale() const { return mInheritScale; }
	/*Scales the node,combining it's current scale with the passed in scaling factor.*/
	void scale(const Vector3& scale);
	void scale(Real x,Real y,Real z);
	/*Moves the node along the Cartesian axes*/
	void translate(const Vector3& d,TransformSpace relativeTo = TS_PARENT);
	void translate(Real x,Real y,Real z,TransformSpace relativeTo = TS_PARENT);
	/*Moves the node along arbitrary axes*/
	void translate(const Matrix3& axes,const Vector3& move,TransformSpace relativeTo = TS_PARENT);
	void translate(const Matrix3& axes, Real x, Real y, Real z, TransformSpace relativeTo = TS_PARENT);
	//Rotate the node around the Z-axis
	void roll(const Radian& angle,TransformSpace relativeTo = TS_LOCAL);
	//Rotate the node around the X-axis
	void pitch(const Radian& angle,TransformSpace relativeTo = TS_LOCAL);
	//Rotate the node around the Y-axis
	void yaw(const Radian& angle,TransformSpace relativeTo = TS_LOCAL);
	//Rotate the node around an aritrary axis using a Quarternion
	void rotate(const Vector3& axis,const Radian& angle,TransformSpace relativeTo = TS_LOCAL);
	//Rotate the node around an aritrary axis using a Quarternion
	void rotate(const Quaternion& q, TransformSpace relativeTo = TS_LOCAL);
	/*Gets a matrix whose columns are the local axes based on the nodes orientation relative to its parent.*/
	Matrix3 getLocalAxes() const;
	//OctreeSceneNode* createChild(const Vector3& translate = Vector3::ZERO,const Quaternion& rotate = Quaternion::IDENTITY);
	OctreeSceneNode* createChild(const String& name,const Vector3& translate = Vector3::ZERO, const Quaternion& rotate = Quaternion::IDENTITY);
	//Adds a child scene node to this node
	void addChild(OctreeSceneNode* child);
	uint16 numChildren() const { return static_cast<uint16>(mChildren.size()); }
	OctreeSceneNode* getChild(unsigned short index) const;
	OctreeSceneNode* getChild(const String& name) const;
	OctreeSceneNode* removeChild(unsigned short index);
	OctreeSceneNode* removeChild(OctreeSceneNode* child);
	OctreeSceneNode* removeChild(const String& name);
	void removeAllChildren();
	//Sets th final world position of the node directly
	void _setDerivedPosition(const Vector3& pos);
	//Sets the final world orientation of the node directly
	void _setDerivedOrientation(const Quaternion& q);
	//Gets the orientation of the node as derived from all parents
	const Quaternion& _getDerivedOrientation() const;
	//Gets the position of the node as derived from all parents;
	const Vector3& _getDerivedPosition() const;
	//Gets the scaling factor of the node as derived from all parents.
	const Vector3& _getDerivedScale() const;
	//*Gets the full transformation matrix for this node*/
	const Matrix4& _getFullTransform() const;
	/*Internal method to update the Node
	Updates this node and any relevant children to incorporate transform etc.*/
	void _update(bool updateChildren,bool parentHasChanged);
	//Gets the local position,relative to this node,of the given world-space position.
	Vector3 convertWorldToLocalPosition(const Vector3& worldPos);
	//Gets the world psotion of a point in the node local space useful for simple transforms that dont require a child node.
	Vector3 convertLocalToWorldPosition(const Vector3& localPos);
	//Gets the local direction,relative to this node,of the given world-space direction.
	Vector3 convertWorldToLocalDirection(const Vector3& worldDir,bool useScale);
	Vector3 convertLocalToWorldDirection(const Vector3& localDir,bool useScale);
	//Gets the local orientation,relative to this node,of the given world-space orientation
	Quaternion convertWorldToLocalOrientation(const Quaternion& worldOrientation);
	//Gets the world orientation of an orientation in the node local space useful for simple transforms that dont require a child node.
	Quaternion convertLocalToWorldOrientation(const Quaternion& localOrientation);
	/*To be called in the event of transform changes to this node that require it's recalculation
	This is not only tags the node state as being dirty,it also requests it's parent to know about it's dirtiness so it will
	get an update next time.
	*/
	void needUpdate(bool forceParentUpdate = false);
	/*Called by children to notify their parent that they need an update*/
	void requestUpdate(OctreeSceneNode* child,bool forceParentUpdate = false);
	/*Called by children to notify their parent that they no longer need an update.*/
	void cancelUpdate(OctreeSceneNode* child);
	/*Queue a 'needUpdate' call to a node safely.*/
	static void queueNeedUpdate(OctreeSceneNode* n);
	/*Process queued 'needUpdate' calls.*/
	static void processQueuedUpdates();
};

#endif
