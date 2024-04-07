#include <Turso3D/Scene/Node.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Scene/Scene.h>

namespace Turso3D
{
	Node::~Node()
	{
		RemoveAllChildren();
	}

	void Node::SetName(const std::string& newName)
	{
		name = newName;
		nameHash = StringHash {newName};
	}

	void Node::SetName(const char* newName)
	{
		name = newName;
		nameHash = StringHash {newName};
	}

	void Node::SetLayer(uint8_t newLayer)
	{
		if (layer < 32) {
			layer = newLayer;
			OnLayerChanged(newLayer);
		} else {
			LOG_ERROR("Can not set layer 32 or higher");
		}
	}

	void Node::SetEnabled(bool enable)
	{
		if (enable != TestFlag(FLAG_ENABLED)) {
			SetFlag(FLAG_ENABLED, enable);
			OnEnabledChanged(enable);
		}
	}

	void Node::SetEnabledRecursive(bool enable)
	{
		SetEnabled(enable);
		for (auto it = children.begin(); it != children.end(); ++it) {
			Node* child = it->get();
			child->SetEnabledRecursive(enable);
		}
	}

	void Node::AddChild(std::unique_ptr<Node> child)
	{
		// Check for illegal or redundant parent assignment
		if (!child || child->parent == this) {
			return;
		}

#ifdef _DEBUG
		// Check for possible illegal or cyclic parent assignment
		if (child.get() == this) {
			LOG_ERROR("Attempted parenting node to self");
			return;
		}

		Node* current = parent;
		while (current) {
			if (current == child.get()) {
				LOG_ERROR("Attempted cyclic node parenting");
				return;
			}
			current = current->parent;
		}
#endif

		child->parent = this;
		child->OnParentSet(this, nullptr);
		child->SetScene(scene);

		auto& newChild = children.emplace_back(std::unique_ptr<Node> {});
		newChild.swap(child);
	}

	std::unique_ptr<Node> Node::RemoveChild(Node* child)
	{
		if (!child || child->parent != this) {
			return {};
		}

		for (auto it = children.begin(); it != children.end(); ++it) {
			if (it->get() == child) {
				std::unique_ptr<Node> node;
				node.swap(*it);

				children.erase(it);

				node->SetScene(nullptr);
				return node;
			}
		}

		return {};
	}

	void Node::RemoveAllChildren()
	{
		for (auto it = children.begin(); it != children.end(); ++it) {
			Node* child = it->get();
			child->parent = nullptr;
			child->SetFlag(FLAG_SPATIALPARENT, false);
			it->reset();
		}
		children.clear();
	}

	void Node::RemoveSelf()
	{
		if (parent) {
			parent->RemoveChild(this);
		}
	}

	size_t Node::NumPersistentChildren() const
	{
		size_t ret = 0;
		for (auto it = children.begin(); it != children.end(); ++it) {
			Node* child = it->get();
			if (!child->TestFlag(FLAG_TEMPORARY)) {
				++ret;
			}
		}
		return ret;
	}

	void Node::SetScene(Scene* newScene)
	{
		Scene* oldScene = scene;
		scene = newScene;
		OnSceneSet(scene, oldScene);

		// Also set for the children
		for (auto it = children.begin(); it != children.end(); ++it) {
			(*it)->SetScene(newScene);
		}
	}

	void Node::OnParentSet(Node*, Node*)
	{
	}

	void Node::OnSceneSet(Scene*, Scene*)
	{
	}

	void Node::OnEnabledChanged(bool)
	{
	}

	void Node::OnLayerChanged(uint8_t)
	{
	}
}
