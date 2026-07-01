#ifndef GAME_CLIENT_COMPONENTS_MENUS_INGAME_SIRIUS_H
#define GAME_CLIENT_COMPONENTS_MENUS_INGAME_SIRIUS_H

#include <game/client/component.h>
#include <game/client/ui.h>

class CMenus;

class CMenusIngameSirius : public CComponentInterfaces
{
public:
	void SetMenu(CMenus *pMenus) { m_pMenus = pMenus; }
	void Render(CUIRect MainView);

private:
	CMenus *m_pMenus = nullptr;
};

#endif
