#include "Panel.h"

#include "../menu/MenuOverlay.h"

void Panel::draw(MenuOverlay * overlay)
{
	overlay->drawRect(glm::vec2(32, 416), glm::vec2(32 + 32, 416 + 32), position, position+size); //menubar
	overlay->flushVerts();

	for (auto c : components)
		c->draw(overlay);

}

bool Panel::click(bool leftButton, const glm::ivec2 & clickPos)
{
	return false;
}

void Panel::onReposition(Component* parent)
{
	for (auto c : components)
	{
		c->onReposition(this);
	}
}