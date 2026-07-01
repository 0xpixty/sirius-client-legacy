#include "menus_ingame_sirius.h"

#include "menus.h"

#include <base/color.h>
#include <base/math.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>
#include <game/teamscore.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace
{
constexpr ColorRGBA BACKGROUND_TOP(0.0f, 0.0f, 0.0f, 0.91f);
constexpr ColorRGBA BACKGROUND_BOTTOM(0.0f, 0.0f, 0.0f, 0.95f);
constexpr ColorRGBA PANEL_FILL_TOP(0.075f, 0.060f, 0.045f, 0.80f);
constexpr ColorRGBA PANEL_FILL_BOTTOM(0.025f, 0.022f, 0.020f, 0.89f);
constexpr ColorRGBA PANEL_BORDER(0.81f, 0.60f, 0.28f, 0.58f);
constexpr ColorRGBA PANEL_GLOW(0.96f, 0.76f, 0.36f, 0.26f);
constexpr ColorRGBA TEXT_MAIN(0.97f, 0.90f, 0.76f, 1.0f);
constexpr ColorRGBA TEXT_DIM(0.84f, 0.77f, 0.66f, 0.94f);
constexpr ColorRGBA TEXT_MUTED(0.61f, 0.56f, 0.51f, 0.90f);
constexpr ColorRGBA SUCCESS_COLOR(0.44f, 0.89f, 0.46f, 1.0f);
constexpr ColorRGBA WARNING_COLOR(0.95f, 0.72f, 0.29f, 1.0f);
constexpr ColorRGBA DANGER_COLOR(0.96f, 0.42f, 0.28f, 1.0f);
constexpr float HEADER_TITLE_SIZE = 32.0f;
constexpr float HEADER_SUBTITLE_SIZE = 10.2f;
constexpr float SECTION_TITLE_SIZE = 10.8f;
constexpr float BODY_TEXT_SIZE = 9.0f;
constexpr float BODY_TEXT_SMALL = 8.1f;
constexpr float PANEL_ROUNDING = 12.0f;

enum class EActionButtonStyle
{
	PRIMARY,
	NORMAL,
	DANGER,
};

struct SInfoRow
{
	const char *m_pIcon;
	const char *m_pLabel;
	std::string m_Value;
	ColorRGBA m_ValueColor = TEXT_MAIN;
};

const char *LocalizeLocation(int Location)
{
	static const char *const s_apLocations[CServerInfo::NUM_LOCS] = {
		"Unknown",
		"AFR",
		"ASI",
		"AUS",
		"EUR",
		"NA",
		"SA",
		"CHN",
	};
	if(Location < 0 || Location >= CServerInfo::NUM_LOCS)
		return Localize("Unknown");
	return Localize(s_apLocations[Location]);
}

std::string FormatRaceTime(int Time)
{
	if(Time <= 0)
		return "--";

	char aBuf[32];
	str_time((int64_t)Time * 100, ETimeFormat::HOURS, aBuf, sizeof(aBuf));
	return aBuf;
}

std::string FormatBestTime(CGameClient *pGameClient)
{
	if(!pGameClient->m_ReceivedDDNetPlayerFinishTimes)
		return "--";

	const int LocalId = pGameClient->m_aLocalIds[g_Config.m_ClDummy];
	if(LocalId < 0)
		return "--";

	const int BestSeconds = pGameClient->m_aClients[LocalId].m_FinishTimeSeconds;
	if(BestSeconds == FinishTime::NOT_FINISHED_MILLIS || BestSeconds == FinishTime::UNSET)
		return "--";

	const int BestMillis = pGameClient->m_aClients[LocalId].m_FinishTimeMillis;
	char aBuf[32];
	str_time((int64_t)BestSeconds * 100 + BestMillis / 10, ETimeFormat::HOURS_CENTISECS, aBuf, sizeof(aBuf));
	return aBuf;
}

std::string FormatCheckpoint(CGameClient *pGameClient, int LocalId)
{
	if(LocalId < 0)
		return "--";

	int TeleCheckpoint = 0;
	if(pGameClient->m_Snap.m_aCharacters[LocalId].m_HasExtendedData)
		TeleCheckpoint = pGameClient->m_Snap.m_aCharacters[LocalId].m_ExtendedData.m_TeleCheckpoint;
	else if(CCharacter *pPredictedChar = pGameClient->m_PredictedWorld.GetCharacterById(LocalId))
		TeleCheckpoint = pPredictedChar->m_TeleCheckpoint;

	if(TeleCheckpoint <= 0)
		return "--";

	char aBuf[32];
	str_format(aBuf, sizeof(aBuf), "%d", TeleCheckpoint);
	return aBuf;
}

std::string ExtractRegionFromServerName(const char *pServerName)
{
	if(pServerName == nullptr || pServerName[0] == '\0')
		return "";

	const char *pFirstSpace = str_find(pServerName, " ");
	if(pFirstSpace == nullptr || pFirstSpace[1] == '\0')
		return "";

	const char *pDash = str_find(pFirstSpace + 1, " - ");
	if(pDash == nullptr || pDash <= pFirstSpace + 1)
		return "";

	const int RegionLength = minimum((int)(pDash - (pFirstSpace + 1)), 15);
	if(RegionLength <= 0)
		return "";

	char aRegion[16];
	str_copy(aRegion, pFirstSpace + 1, RegionLength + 1);
	str_utf8_trim_right(aRegion);
	return aRegion;
}

std::string TrimCopy(const std::string &Text)
{
	if(Text.empty())
		return Text;

	char aBuf[512];
	str_copy(aBuf, Text.c_str(), sizeof(aBuf));
	const char *pTrimmed = str_utf8_skip_whitespaces(aBuf);
	char aTrimmed[512];
	str_copy(aTrimmed, pTrimmed, sizeof(aTrimmed));
	str_utf8_trim_right(aTrimmed);
	return aTrimmed;
}

std::string FormatMotdLabel(const std::string &Key)
{
	if(Key.empty())
		return "";

	if(str_find_nocase(Key.c_str(), "server") || str_find_nocase(Key.c_str(), "name") || str_find_nocase(Key.c_str(), "host"))
		return "Server";
	if(str_find_nocase(Key.c_str(), "mode") || str_find_nocase(Key.c_str(), "type") || str_find_nocase(Key.c_str(), "gametype"))
		return "Mode";
	if(str_find_nocase(Key.c_str(), "map"))
		return "Map";
	if(str_find_nocase(Key.c_str(), "region") || str_find_nocase(Key.c_str(), "location"))
		return "Region";
	if(str_find_nocase(Key.c_str(), "ping") || str_find_nocase(Key.c_str(), "latency"))
		return "Ping";
	if(str_find_nocase(Key.c_str(), "players") || str_find_nocase(Key.c_str(), "online"))
		return "Players";
	if(str_find_nocase(Key.c_str(), "version"))
		return "Version";
	if(str_find_nocase(Key.c_str(), "description") || str_find_nocase(Key.c_str(), "desc") || str_find_nocase(Key.c_str(), "motd"))
		return "Description";

	return Key;
}

int ServerLabelOrder(const char *pLabel)
{
	static const std::array<const char *, 6> s_apOrderedLabels = {
		"Название сервера",
		"Режим",
		"Карта",
		"Регион",
		"Ваш пинг",
		"Игроков онлайн",
	};
	for(size_t i = 0; i < s_apOrderedLabels.size(); ++i)
	{
		if(str_comp(pLabel, s_apOrderedLabels[i]) == 0)
			return (int)i;
	}
	return (int)s_apOrderedLabels.size();
}

std::vector<SInfoRow> BuildServerInfoRows(CGameClient *pGameClient, const CServerInfo &ServerInfo)
{
	char aPlayers[32];
	str_format(aPlayers, sizeof(aPlayers), "%d / %d", pGameClient->m_Snap.m_NumPlayers, ServerInfo.m_MaxClients);

	const int LocalId = pGameClient->m_aLocalIds[g_Config.m_ClDummy];
	int LocalPing = -1;
	if(LocalId >= 0)
	{
		if(const CNetObj_PlayerInfo *pLocalInfo = pGameClient->m_Snap.m_apPlayerInfos[LocalId])
			LocalPing = pLocalInfo->m_Latency;
	}
	if(LocalPing < 0)
		LocalPing = ServerInfo.m_Latency;

	const std::string DerivedRegion = ExtractRegionFromServerName(ServerInfo.m_aName);
	const std::string RegionValue = !DerivedRegion.empty() ? DerivedRegion : LocalizeLocation(ServerInfo.m_Location);
	const ColorRGBA PingColor = LocalPing >= 0 ? (LocalPing < 80 ? SUCCESS_COLOR : (LocalPing < 140 ? WARNING_COLOR : DANGER_COLOR)) : TEXT_MAIN;

	std::vector<SInfoRow> vRows = {
		{FontIcon::INFO, "Название сервера", ServerInfo.m_aName[0] ? ServerInfo.m_aName : "--"},
		{FontIcon::SIRIUS, "Режим", ServerInfo.m_aGameType[0] ? ServerInfo.m_aGameType : "--"},
		{FontIcon::MAP, "Карта", ServerInfo.m_aMap[0] ? ServerInfo.m_aMap : "--"},
		{FontIcon::EARTH_AMERICAS, "Регион", RegionValue},
		{FontIcon::ARROWS_LEFT_RIGHT, "Ваш пинг", LocalPing >= 0 ? std::to_string(LocalPing) : std::string("--"), PingColor},
		{FontIcon::USERS, "Игроков онлайн", aPlayers},
	};

	const char *pMotd = pGameClient->m_Motd.ServerMotd();
	if(pMotd == nullptr || pMotd[0] == '\0')
		return vRows;

	std::vector<SInfoRow> vParsedRows;
	std::string MotdText = pMotd;
	size_t LineStart = 0;
	while(LineStart <= MotdText.size())
	{
		size_t LineEnd = MotdText.find('\n', LineStart);
		if(LineEnd == std::string::npos)
			LineEnd = MotdText.size();

		std::string Line = TrimCopy(MotdText.substr(LineStart, LineEnd - LineStart));
		LineStart = LineEnd + 1;
		if(Line.empty())
			continue;

		const size_t Separator = Line.find(':');
		if(Separator == std::string::npos)
			continue;

		std::string Key = FormatMotdLabel(TrimCopy(Line.substr(0, Separator)));
		std::string Value = TrimCopy(Line.substr(Separator + 1));
		if(Key.empty() || Value.empty() || Key == "Version" || Key == "Description")
			continue;

		const char *pLabel = nullptr;
		const char *pIcon = FontIcon::INFO;
		ColorRGBA ValueColor = TEXT_MAIN;
		if(Key == "Server")
			pLabel = "Название сервера";
		else if(Key == "Mode")
		{
			pLabel = "Режим";
			pIcon = FontIcon::SIRIUS;
		}
		else if(Key == "Map")
		{
			pLabel = "Карта";
			pIcon = FontIcon::MAP;
		}
		else if(Key == "Region")
		{
			pLabel = "Регион";
			pIcon = FontIcon::EARTH_AMERICAS;
		}
		else if(Key == "Ping")
		{
			pLabel = "Ваш пинг";
			pIcon = FontIcon::ARROWS_LEFT_RIGHT;
			const int Ping = str_toint(Value.c_str());
			ValueColor = Ping < 80 ? SUCCESS_COLOR : (Ping < 140 ? WARNING_COLOR : DANGER_COLOR);
		}
		else if(Key == "Players")
		{
			pLabel = "Игроков онлайн";
			pIcon = FontIcon::USERS;
		}

		if(pLabel == nullptr)
			continue;

		bool Replaced = false;
		for(SInfoRow &Row : vParsedRows)
		{
			if(str_comp(Row.m_pLabel, pLabel) == 0)
			{
				Row.m_pIcon = pIcon;
				Row.m_Value = Value;
				Row.m_ValueColor = ValueColor;
				Replaced = true;
				break;
			}
		}
		if(!Replaced)
			vParsedRows.push_back({pIcon, pLabel, Value, ValueColor});
	}

	for(const SInfoRow &FallbackRow : vRows)
	{
		const bool Exists = std::any_of(vParsedRows.begin(), vParsedRows.end(), [&](const SInfoRow &Row) {
			return str_comp(Row.m_pLabel, FallbackRow.m_pLabel) == 0;
		});
		if(!Exists)
			vParsedRows.push_back(FallbackRow);
	}

	std::stable_sort(vParsedRows.begin(), vParsedRows.end(), [](const SInfoRow &A, const SInfoRow &B) {
		return ServerLabelOrder(A.m_pLabel) < ServerLabelOrder(B.m_pLabel);
	});

	return vParsedRows;
}

int64_t GetClientRaceTimeValue(CGameClient *pGameClient, int ClientId)
{
	if(ClientId < 0)
		return -1;

	if(pGameClient->m_ReceivedDDNetPlayerFinishTimes)
	{
		const auto &Client = pGameClient->m_aClients[ClientId];
		if(Client.m_FinishTimeSeconds == FinishTime::UNSET || Client.m_FinishTimeSeconds == FinishTime::NOT_FINISHED_MILLIS)
			return -1;
		return (int64_t)Client.m_FinishTimeSeconds * 1000 + Client.m_FinishTimeMillis;
	}

	const CNetObj_PlayerInfo *pInfo = pGameClient->m_Snap.m_apPlayerInfos[ClientId];
	if(pInfo == nullptr || !pGameClient->m_GameInfo.m_TimeScore)
		return -1;

	if(pInfo->m_Score == FinishTime::UNSET || pInfo->m_Score == FinishTime::NOT_FINISHED_TIMESCORE)
		return -1;
	return (int64_t)absolute(pInfo->m_Score) * 10;
}

bool RenderClientRaceTime(CGameClient *pGameClient, CUi *pUi, const CUIRect &Rect, float FontSize, int ClientId)
{
	if(ClientId < 0)
		return false;

	CUIRect RenderRect = Rect;
	const float TimeWidth = minimum(Rect.w, pUi->TextRender()->TextWidth(FontSize, "00:00:00") + 8.0f);
	RenderRect.x += (RenderRect.w - TimeWidth) * 0.5f;
	RenderRect.w = TimeWidth;

	if(pGameClient->m_ReceivedDDNetPlayerFinishTimes)
	{
		const auto &Client = pGameClient->m_aClients[ClientId];
		if(Client.m_FinishTimeSeconds == FinishTime::UNSET || Client.m_FinishTimeSeconds == FinishTime::NOT_FINISHED_MILLIS)
			return false;

		pUi->RenderTime(RenderRect, FontSize, Client.m_FinishTimeSeconds, false, Client.m_FinishTimeMillis, pGameClient->m_ReceivedDDNetPlayerFinishTimesMillis);
		return true;
	}

	const CNetObj_PlayerInfo *pInfo = pGameClient->m_Snap.m_apPlayerInfos[ClientId];
	if(pInfo == nullptr || !pGameClient->m_GameInfo.m_TimeScore)
		return false;

	if(pInfo->m_Score == FinishTime::UNSET || pInfo->m_Score == FinishTime::NOT_FINISHED_TIMESCORE)
		return false;

	pUi->RenderTime(RenderRect, FontSize, pInfo->m_Score, false, -1, false);
	return true;
}

int GetDisplayPing(CGameClient *pGameClient, const CNetObj_PlayerInfo *pInfo, int ClientId, int LocalId, int FallbackPing = -1)
{
	int Ping = -1;
	if(ClientId >= 0)
	{
		if(const CNetObj_PlayerInfo *pSnapInfo = pGameClient->m_Snap.m_apPlayerInfos[ClientId])
			Ping = pSnapInfo->m_Latency;
	}
	if((Ping <= 0 || Ping > 999) && pInfo != nullptr)
		Ping = pInfo->m_Latency;
	if((Ping <= 0 || Ping > 999) && ClientId == LocalId && pGameClient->m_Snap.m_pLocalInfo != nullptr)
		Ping = pGameClient->m_Snap.m_pLocalInfo->m_Latency;
	if((Ping <= 0 || Ping > 999) && FallbackPing >= 0)
		Ping = FallbackPing;
	return Ping >= 0 && Ping <= 999 ? Ping : -1;
}

void DrawPanel(IGraphics *pGraphics, const CUIRect &Rect, float Rounding, ColorRGBA BorderColor = PANEL_BORDER, ColorRGBA GlowColor = PANEL_GLOW)
{
	pGraphics->DrawRectGlow(Rect.x, Rect.y, Rect.w, Rect.h, GlowColor, 11.0f, IGraphics::CORNER_ALL, Rounding, 4);
	pGraphics->DrawRect4(Rect.x, Rect.y, Rect.w, Rect.h, PANEL_FILL_TOP, PANEL_FILL_TOP, PANEL_FILL_BOTTOM, PANEL_FILL_BOTTOM, IGraphics::CORNER_ALL, Rounding);
	pGraphics->DrawRectOutline(Rect.x, Rect.y, Rect.w, Rect.h, BorderColor, IGraphics::CORNER_ALL, IGraphics::SIDE_ALL, Rounding);
	pGraphics->DrawRect(Rect.x + 1.0f, Rect.y + 1.0f, Rect.w - 2.0f, Rect.h * 0.18f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.012f), IGraphics::CORNER_T, Rounding);
}

void DrawCornerAccents(IGraphics *pGraphics, const CUIRect &Rect, float Size, ColorRGBA Color)
{
	const float T = 1.2f;
	pGraphics->DrawRect(Rect.x + 10.0f, Rect.y + 10.0f, Size, T, Color, IGraphics::CORNER_NONE, 0.0f);
	pGraphics->DrawRect(Rect.x + 10.0f, Rect.y + 10.0f, T, Size, Color, IGraphics::CORNER_NONE, 0.0f);
	pGraphics->DrawRect(Rect.x + Rect.w - 10.0f - Size, Rect.y + 10.0f, Size, T, Color, IGraphics::CORNER_NONE, 0.0f);
	pGraphics->DrawRect(Rect.x + Rect.w - 10.0f, Rect.y + 10.0f, T, Size, Color, IGraphics::CORNER_NONE, 0.0f);
	pGraphics->DrawRect(Rect.x + 10.0f, Rect.y + Rect.h - 10.0f, Size, T, Color, IGraphics::CORNER_NONE, 0.0f);
	pGraphics->DrawRect(Rect.x + 10.0f, Rect.y + Rect.h - 10.0f - Size, T, Size, Color, IGraphics::CORNER_NONE, 0.0f);
	pGraphics->DrawRect(Rect.x + Rect.w - 10.0f - Size, Rect.y + Rect.h - 10.0f, Size, T, Color, IGraphics::CORNER_NONE, 0.0f);
	pGraphics->DrawRect(Rect.x + Rect.w - 10.0f, Rect.y + Rect.h - 10.0f - Size, T, Size, Color, IGraphics::CORNER_NONE, 0.0f);
}

void RenderAmbientLights(IGraphics *pGraphics, const CUIRect &Rect)
{
	static const std::array<vec2, 12> s_aRelativePoints = {
		vec2(0.06f, 0.08f), vec2(0.18f, 0.03f), vec2(0.31f, 0.12f), vec2(0.48f, 0.05f),
		vec2(0.67f, 0.09f), vec2(0.84f, 0.04f), vec2(0.91f, 0.18f), vec2(0.78f, 0.33f),
		vec2(0.58f, 0.24f), vec2(0.41f, 0.37f), vec2(0.22f, 0.30f), vec2(0.11f, 0.44f),
	};

	for(const vec2 &RelativePoint : s_aRelativePoints)
	{
		const float X = Rect.x + Rect.w * RelativePoint.x;
		const float Y = Rect.y + Rect.h * RelativePoint.y;
		pGraphics->DrawRectGlow(X - 16.0f, Y - 16.0f, 32.0f, 32.0f, PANEL_GLOW.WithAlpha(0.16f), 32.0f, IGraphics::CORNER_ALL, 16.0f, 6);
		pGraphics->DrawRectGlow(X - 7.0f, Y - 7.0f, 14.0f, 14.0f, PANEL_GLOW.WithAlpha(0.44f), 18.0f, IGraphics::CORNER_ALL, 7.0f, 5);
		pGraphics->DrawRect(X - 2.0f, Y - 2.0f, 4.0f, 4.0f, TEXT_MAIN.WithAlpha(0.95f), IGraphics::CORNER_ALL, 2.0f);
	}

	static const std::array<vec2, 14> s_aStarSeeds = {
		vec2(0.10f, 0.16f), vec2(0.15f, 0.28f), vec2(0.23f, 0.11f), vec2(0.33f, 0.22f),
		vec2(0.41f, 0.15f), vec2(0.51f, 0.26f), vec2(0.59f, 0.12f), vec2(0.68f, 0.20f),
		vec2(0.74f, 0.30f), vec2(0.81f, 0.14f), vec2(0.87f, 0.24f), vec2(0.27f, 0.34f),
		vec2(0.46f, 0.33f), vec2(0.63f, 0.35f),
	};

	const float T = (float)time_get() / (float)time_freq();
	std::array<vec2, s_aStarSeeds.size()> aAnimatedStars;
	for(size_t i = 0; i < s_aStarSeeds.size(); ++i)
	{
		const float Phase = T * (0.12f + 0.015f * (float)(i % 5)) + (float)i * 0.73f;
		const float DriftX = sinf(Phase * 1.37f) * 0.010f;
		const float DriftY = cosf(Phase * 1.11f) * 0.014f;
		aAnimatedStars[i] = vec2(Rect.x + Rect.w * (s_aStarSeeds[i].x + DriftX), Rect.y + Rect.h * (s_aStarSeeds[i].y + DriftY));
	}

	pGraphics->LinesBegin();
	for(size_t i = 0; i < aAnimatedStars.size(); ++i)
	{
		for(size_t j = i + 1; j < aAnimatedStars.size(); ++j)
		{
			const float Distance = distance(aAnimatedStars[i], aAnimatedStars[j]);
			const float MaxDistance = 120.0f;
			if(Distance > MaxDistance)
				continue;

			const float Alpha = (1.0f - Distance / MaxDistance) * 0.10f;
			pGraphics->SetColor(PANEL_GLOW.WithAlpha(Alpha));
			IGraphics::CLineItem Line(aAnimatedStars[i], aAnimatedStars[j]);
			pGraphics->LinesDraw(&Line, 1);
		}
	}
	pGraphics->LinesEnd();

	for(const vec2 &Star : aAnimatedStars)
	{
		pGraphics->DrawRectGlow(Star.x - 10.0f, Star.y - 10.0f, 20.0f, 20.0f, PANEL_GLOW.WithAlpha(0.10f), 22.0f, IGraphics::CORNER_ALL, 10.0f, 5);
		pGraphics->DrawRectGlow(Star.x - 4.0f, Star.y - 4.0f, 8.0f, 8.0f, PANEL_GLOW.WithAlpha(0.26f), 10.0f, IGraphics::CORNER_ALL, 4.0f, 4);
		pGraphics->DrawRect(Star.x - 1.0f, Star.y - 1.0f, 2.0f, 2.0f, TEXT_MAIN.WithAlpha(0.85f), IGraphics::CORNER_ALL, 1.0f);
	}
}

void RenderSectionHeader(CMenus *pMenus, IGraphics *pGraphics, CUi *pUi, ITextRender *pTextRender, CUIRect Rect, const char *pIcon, const char *pTitle)
{
	CUIRect Icon, Title, Line;
	Rect.VSplitLeft(24.0f, &Icon, &Rect);
	Rect.VSplitLeft(6.0f, nullptr, &Rect);
	Rect.VSplitLeft(std::min(Rect.w, pTextRender->TextWidth(SECTION_TITLE_SIZE, pTitle, -1, -1.0f) + 4.0f), &Title, &Rect);
	Rect.VSplitLeft(8.0f, nullptr, &Rect);
	Line = Rect;

	pTextRender->TextColor(TEXT_MAIN);
	pMenus->RenderFontIcon(Icon, pIcon, 13.0f, TEXTALIGN_MC);
	pUi->DoLabel(&Title, pTitle, SECTION_TITLE_SIZE, TEXTALIGN_ML);
	pGraphics->DrawRect4(Line.x, Line.y + Line.h * 0.5f, Line.w, 1.0f,
		PANEL_BORDER.WithAlpha(0.0f), PANEL_BORDER.WithAlpha(0.60f), PANEL_BORDER.WithAlpha(0.0f), PANEL_BORDER.WithAlpha(0.0f),
		IGraphics::CORNER_NONE, 0.0f);
	pTextRender->TextColor(pTextRender->DefaultTextColor());
}

void RenderInfoRows(CMenus *pMenus, IGraphics *pGraphics, CUi *pUi, ITextRender *pTextRender, const CUIRect &Rect, const std::vector<SInfoRow> &vRows, float RowHeight)
{
	CUIRect RowsRect = Rect;
	for(const SInfoRow &Row : vRows)
	{
		if(RowsRect.h < RowHeight)
			break;

		CUIRect RowRect, Divider;
		RowsRect.HSplitTop(RowHeight, &RowRect, &RowsRect);
		RowsRect.HSplitTop(2.0f, &Divider, &RowsRect);

		CUIRect Icon, Label, Value;
		RowRect.VSplitLeft(18.0f, &Icon, &RowRect);
		RowRect.VSplitLeft(5.0f, nullptr, &RowRect);
		RowRect.VSplitLeft(RowRect.w * 0.47f, &Label, &Value);

		pTextRender->TextColor(TEXT_DIM);
		pMenus->RenderFontIcon(Icon, Row.m_pIcon, BODY_TEXT_SIZE, TEXTALIGN_MC);
		pUi->DoLabel(&Label, Row.m_pLabel, BODY_TEXT_SIZE, TEXTALIGN_ML);
		pTextRender->TextColor(Row.m_ValueColor);
		pUi->DoLabel(&Value, Row.m_Value.c_str(), BODY_TEXT_SIZE, TEXTALIGN_MR);
		pTextRender->TextColor(pTextRender->DefaultTextColor());

		if(Divider.h > 0.0f)
			pGraphics->DrawRect(Divider.x + 4.0f, Divider.y + Divider.h * 0.5f, Divider.w - 8.0f, 1.0f, PANEL_BORDER.WithAlpha(0.12f), IGraphics::CORNER_NONE, 0.0f);
	}
}

void RenderInfoTimeRow(CMenus *pMenus, IGraphics *pGraphics, CUi *pUi, ITextRender *pTextRender, const CUIRect &Rect, const char *pIcon, const char *pLabel, int Seconds, int Millis, bool TrueMilliseconds, float RowHeight, ColorRGBA ValueColor = TEXT_MAIN)
{
	if(Seconds < 0)
		return;

	CUIRect RowRect, Divider;
	CUIRect RowsRect = Rect;
	RowsRect.HSplitTop(RowHeight, &RowRect, &RowsRect);
	RowsRect.HSplitTop(2.0f, &Divider, &RowsRect);

	CUIRect Icon, Label, Value;
	RowRect.VSplitLeft(18.0f, &Icon, &RowRect);
	RowRect.VSplitLeft(5.0f, nullptr, &RowRect);
	RowRect.VSplitLeft(RowRect.w * 0.47f, &Label, &Value);

	pTextRender->TextColor(TEXT_DIM);
	pMenus->RenderFontIcon(Icon, pIcon, BODY_TEXT_SIZE, TEXTALIGN_MC);
	pUi->DoLabel(&Label, pLabel, BODY_TEXT_SIZE, TEXTALIGN_ML);
	pTextRender->TextColor(ValueColor);
	pUi->RenderTime(Value, BODY_TEXT_SIZE, Seconds, false, Millis, TrueMilliseconds);
	pTextRender->TextColor(pTextRender->DefaultTextColor());

	if(Divider.h > 0.0f)
		pGraphics->DrawRect(Divider.x + 4.0f, Divider.y + Divider.h * 0.5f, Divider.w - 8.0f, 1.0f, PANEL_BORDER.WithAlpha(0.12f), IGraphics::CORNER_NONE, 0.0f);
}

void RenderPreviewMap(CMenus *pMenus, IGraphics *pGraphics, const CServerInfo &ServerInfo, CUIRect Rect)
{
	pGraphics->DrawRectGlow(Rect.x, Rect.y, Rect.w, Rect.h, PANEL_GLOW.WithAlpha(0.20f), 12.0f, IGraphics::CORNER_ALL, 10.0f, 4);
	pGraphics->DrawRect4(Rect.x, Rect.y, Rect.w, Rect.h,
		ColorRGBA(0.07f, 0.07f, 0.09f, 0.96f), ColorRGBA(0.07f, 0.07f, 0.09f, 0.96f),
		ColorRGBA(0.02f, 0.02f, 0.03f, 0.98f), ColorRGBA(0.02f, 0.02f, 0.03f, 0.98f),
		IGraphics::CORNER_ALL, 10.0f);
	pGraphics->DrawRectOutline(Rect.x, Rect.y, Rect.w, Rect.h, PANEL_BORDER.WithAlpha(0.46f), IGraphics::CORNER_ALL, IGraphics::SIDE_ALL, 10.0f);

	const float BaseY = Rect.y + Rect.h * 0.70f;
	const float Left = Rect.x + 18.0f;
	const float Width = Rect.w - 36.0f;
	const float NodeStep = Width / 5.0f;
	const float HeightUnit = Rect.h / 8.0f;
	const int Seed = absolute(ServerInfo.m_MapCrc) + ServerInfo.m_MapSize;

	std::array<vec2, 6> aNodes;
	for(size_t i = 0; i < aNodes.size(); ++i)
	{
		const float YOffset = ((Seed >> (int)(i * 2)) & 0x3) * HeightUnit * 0.55f;
		aNodes[i] = vec2(Left + NodeStep * i, BaseY - YOffset);
	}

	pGraphics->LinesBegin();
	pGraphics->SetColor(PANEL_BORDER.WithAlpha(0.34f));
	for(size_t i = 0; i + 1 < aNodes.size(); ++i)
	{
		IGraphics::CLineItem Segment(aNodes[i], aNodes[i + 1]);
		pGraphics->LinesDraw(&Segment, 1);
	}
	pGraphics->LinesEnd();

	for(const vec2 &Node : aNodes)
	{
		pGraphics->DrawRectGlow(Node.x - 4.0f, Node.y - 4.0f, 8.0f, 8.0f, PANEL_GLOW.WithAlpha(0.70f), 12.0f, IGraphics::CORNER_ALL, 4.0f, 4);
		pGraphics->DrawRect(Node.x - 1.5f, Node.y - 1.5f, 3.0f, 3.0f, TEXT_MAIN.WithAlpha(0.88f), IGraphics::CORNER_ALL, 1.5f);
	}

	CUIRect StartIcon = {Rect.x + 9.0f, BaseY - 12.0f, 16.0f, 16.0f};
	CUIRect EndIcon = {Rect.x + Rect.w - 24.0f, aNodes.back().y - 12.0f, 16.0f, 16.0f};
	pMenus->RenderFontIcon(StartIcon, FontIcon::PLAY, 11.0f, TEXTALIGN_MC);
	pMenus->RenderFontIcon(EndIcon, FontIcon::FLAG_CHECKERED, 11.0f, TEXTALIGN_MC);
}

bool DoActionButton(
	CMenus *pMenus,
	IGraphics *pGraphics,
	CUi *pUi,
	ITextRender *pTextRender,
	const CUIRect &Rect,
	CButtonContainer *pButton,
	const char *pIcon,
	const char *pLabel,
	const char *pHotkey,
	EActionButtonStyle Style,
	bool Enabled)
{
	(void)pHotkey;

	const bool Hovered = Enabled && pUi->HotItem() == pButton;
	ColorRGBA FillTop = ColorRGBA(0.12f, 0.11f, 0.10f, Enabled ? 0.92f : 0.55f);
	ColorRGBA FillBottom = ColorRGBA(0.08f, 0.07f, 0.06f, Enabled ? 0.95f : 0.60f);
	ColorRGBA Border = PANEL_BORDER.WithAlpha(Enabled ? (Hovered ? 0.78f : 0.38f) : 0.18f);
	ColorRGBA Glow = PANEL_GLOW.WithAlpha(Enabled ? (Hovered ? 0.42f : 0.20f) : 0.0f);
	ColorRGBA TextColor = Enabled ? TEXT_MAIN : TEXT_MUTED;

	if(Style == EActionButtonStyle::PRIMARY)
	{
		FillTop = ColorRGBA(0.72f, 0.53f, 0.22f, Enabled ? 0.94f : 0.60f);
		FillBottom = ColorRGBA(0.53f, 0.36f, 0.12f, Enabled ? 0.98f : 0.66f);
		Border = ColorRGBA(0.98f, 0.78f, 0.42f, Enabled ? 0.84f : 0.30f);
		Glow = ColorRGBA(0.98f, 0.78f, 0.42f, Enabled ? 0.50f : 0.0f);
		TextColor = Enabled ? ColorRGBA(1.0f, 0.96f, 0.90f, 1.0f) : TEXT_MUTED;
	}
	else if(Style == EActionButtonStyle::DANGER)
	{
		FillTop = ColorRGBA(0.36f, 0.11f, 0.08f, Enabled ? 0.95f : 0.60f);
		FillBottom = ColorRGBA(0.25f, 0.07f, 0.05f, Enabled ? 0.98f : 0.66f);
		Border = ColorRGBA(0.94f, 0.42f, 0.28f, Enabled ? 0.72f : 0.24f);
		Glow = ColorRGBA(0.96f, 0.45f, 0.30f, Enabled ? 0.42f : 0.0f);
		TextColor = Enabled ? ColorRGBA(1.0f, 0.58f, 0.40f, 1.0f) : TEXT_MUTED;
	}

	pGraphics->DrawRectGlow(Rect.x, Rect.y, Rect.w, Rect.h, Glow, Hovered ? 16.0f : 10.0f, IGraphics::CORNER_ALL, 9.0f, 6);
	pGraphics->DrawRect4(Rect.x, Rect.y, Rect.w, Rect.h, FillTop, FillTop, FillBottom, FillBottom, IGraphics::CORNER_ALL, 9.0f);
	pGraphics->DrawRectOutline(Rect.x, Rect.y, Rect.w, Rect.h, Border, IGraphics::CORNER_ALL, IGraphics::SIDE_ALL, 9.0f);

	CUIRect IconRect = Rect;
	CUIRect LabelRect = Rect;
	IconRect.VSplitLeft(34.0f, &IconRect, &LabelRect);
	LabelRect.VSplitLeft(9.0f, nullptr, &LabelRect);
	LabelRect.VSplitRight(9.0f, &LabelRect, nullptr);

	pTextRender->TextColor(TextColor);
	pMenus->RenderFontIcon(IconRect, pIcon, 13.5f, TEXTALIGN_MC);
	pUi->DoLabel(&LabelRect, pLabel, 10.5f, TEXTALIGN_MC);
	pTextRender->TextColor(pTextRender->DefaultTextColor());

	if(!Enabled)
		return false;

	return pUi->DoButtonLogic(pButton, 0, &Rect, BUTTONFLAG_LEFT);
}

void RenderClientPreview(CRenderTools *pRenderTools, CUIRect Rect, const CGameClient::CClientData &ClientData)
{
	CTeeRenderInfo TeeInfo = ClientData.m_RenderInfo;
	TeeInfo.m_Size = Rect.h * 0.92f;

	const CAnimState *pIdleState = CAnimState::GetIdle();
	vec2 OffsetToMid;
	CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
	vec2 TeePos(Rect.x + Rect.w * 0.5f, Rect.y + Rect.h * 0.5f + OffsetToMid.y);
	pRenderTools->RenderTee(pIdleState, &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeePos);
}
} // namespace

void CMenusIngameSirius::Render(CUIRect MainView)
{
	(void)MainView;
	if(m_pMenus == nullptr)
		return;

	CServerInfo ServerInfo;
	Client()->GetServerInfo(&ServerInfo);

	const int LocalId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	const bool HasLocalInfo = GameClient()->m_Snap.m_pLocalInfo != nullptr;
	const bool Paused = GameClient()->m_Snap.m_LocalClientId >= 0 && GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_Paused;
	const bool Spec = GameClient()->m_Snap.m_LocalClientId >= 0 && GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_Spec;

	CUIRect Root = *Ui()->Screen();
	Graphics()->DrawRect4(Root.x, Root.y, Root.w, Root.h, BACKGROUND_TOP, BACKGROUND_TOP, BACKGROUND_BOTTOM, BACKGROUND_BOTTOM, IGraphics::CORNER_NONE, 0.0f);
	Graphics()->DrawRectOutline(Root.x, Root.y, Root.w, Root.h, PANEL_BORDER.WithAlpha(0.42f), IGraphics::CORNER_NONE, IGraphics::SIDE_ALL, 0.0f);
	RenderAmbientLights(Graphics(), Root);
	DrawCornerAccents(Graphics(), Root, 12.0f, PANEL_BORDER.WithAlpha(0.75f));

	CUIRect SafeRoot = Root;
	SafeRoot.Margin(16.0f, &SafeRoot);

	CUIRect Header, Content, Footer;
	SafeRoot.HSplitTop(90.0f, &Header, &Content);
	Content.HSplitBottom(48.0f, &Content, &Footer);
	Content.Margin(14.0f, &Content);

	{
		CUIRect TitleRect, SubtitleRect;
		Header.HSplitTop(54.0f, &TitleRect, &Header);
		Header.HSplitTop(18.0f, &SubtitleRect, &Header);

		TextRender()->TextColor(TEXT_MAIN);
		Ui()->DoLabel(&TitleRect, "Пауза", HEADER_TITLE_SIZE, TEXTALIGN_MC);
		TextRender()->TextColor(TEXT_DIM);
		Ui()->DoLabel(&SubtitleRect, "Sirius Client   •   Меню паузы", HEADER_SUBTITLE_SIZE, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	CUIRect MainArea, ActionsWrap;
	Content.VSplitRight(190.0f, &MainArea, &ActionsWrap);

	CUIRect ActionsGap, ActionsColumn;
	ActionsWrap.VSplitLeft(12.0f, &ActionsGap, &ActionsColumn);

	CUIRect TopBand, PlayersPanel;
	MainArea.HSplitTop(236.0f, &TopBand, &PlayersPanel);
	PlayersPanel.HSplitTop(10.0f, nullptr, &PlayersPanel);

	CUIRect ServerPanel, CenterColumn;
	TopBand.VSplitLeft(360.0f, &ServerPanel, &CenterColumn);
	CenterColumn.VSplitLeft(10.0f, nullptr, &CenterColumn);

	CUIRect MapPanel, SessionPanel;
	CenterColumn.HSplitTop((TopBand.h - 10.0f) * 0.47f, &MapPanel, &SessionPanel);
	SessionPanel.HSplitTop(10.0f, nullptr, &SessionPanel);

	DrawPanel(Graphics(), ServerPanel, PANEL_ROUNDING);
	DrawPanel(Graphics(), MapPanel, PANEL_ROUNDING);
	DrawPanel(Graphics(), SessionPanel, PANEL_ROUNDING);
	DrawPanel(Graphics(), PlayersPanel, PANEL_ROUNDING);
	DrawPanel(Graphics(), ActionsColumn, 17.0f);
	DrawCornerAccents(Graphics(), ServerPanel, 7.0f, PANEL_BORDER.WithAlpha(0.60f));
	DrawCornerAccents(Graphics(), MapPanel, 7.0f, PANEL_BORDER.WithAlpha(0.60f));
	DrawCornerAccents(Graphics(), SessionPanel, 7.0f, PANEL_BORDER.WithAlpha(0.60f));
	DrawCornerAccents(Graphics(), PlayersPanel, 7.0f, PANEL_BORDER.WithAlpha(0.50f));
	DrawCornerAccents(Graphics(), ActionsColumn, 7.0f, PANEL_BORDER.WithAlpha(0.60f));

	{
		CUIRect Inner = ServerPanel;
		Inner.Margin(10.0f, &Inner);
		CUIRect HeaderRect, RowsRect;
		Inner.HSplitTop(14.0f, &HeaderRect, &RowsRect);
		RowsRect.HSplitTop(6.0f, nullptr, &RowsRect);

		RenderSectionHeader(m_pMenus, Graphics(), Ui(), TextRender(), HeaderRect, FontIcon::INFO, "ИНФОРМАЦИЯ О СЕРВЕРЕ");
		const std::vector<SInfoRow> vRows = BuildServerInfoRows(GameClient(), ServerInfo);
		RenderInfoRows(m_pMenus, Graphics(), Ui(), TextRender(), RowsRect, vRows, 14.0f);
	}

	{
		CUIRect Inner = MapPanel;
		Inner.Margin(10.0f, &Inner);
		CUIRect HeaderRect, ContentRect;
		Inner.HSplitTop(14.0f, &HeaderRect, &ContentRect);
		ContentRect.HSplitTop(6.0f, nullptr, &ContentRect);

		RenderSectionHeader(m_pMenus, Graphics(), Ui(), TextRender(), HeaderRect, FontIcon::MAP, "ИНФОРМАЦИЯ О КАРТЕ");

		CUIRect RowsRect, PreviewRect;
		ContentRect.VSplitLeft(ContentRect.w * 0.40f, &RowsRect, &PreviewRect);
		PreviewRect.VSplitLeft(8.0f, nullptr, &PreviewRect);

		CUIRect MapRowsRect = RowsRect;
		CUIRect NameRowRect, BestTimeRect, RemainingRows;
		MapRowsRect.HSplitTop(16.0f, &NameRowRect, &MapRowsRect);
		MapRowsRect.HSplitTop(16.0f, &BestTimeRect, &RemainingRows);

		std::vector<SInfoRow> vRows;
		vRows.push_back({FontIcon::MAP, "Название карты", ServerInfo.m_aMap[0] ? ServerInfo.m_aMap : "--"});
		RenderInfoRows(m_pMenus, Graphics(), Ui(), TextRender(), NameRowRect, vRows, 14.0f);
		if(GameClient()->m_MapBestTimeSeconds != FinishTime::UNSET && GameClient()->m_MapBestTimeSeconds != FinishTime::NOT_FINISHED_MILLIS)
			RenderInfoTimeRow(m_pMenus, Graphics(), Ui(), TextRender(), BestTimeRect, FontIcon::STAR, "Лучшее время", GameClient()->m_MapBestTimeSeconds, GameClient()->m_MapBestTimeMillis, GameClient()->m_ReceivedDDNetPlayerFinishTimesMillis, 14.0f);
		else
			RenderInfoRows(m_pMenus, Graphics(), Ui(), TextRender(), BestTimeRect, {{FontIcon::STAR, "Лучшее время", "--"}}, 14.0f);

		CUIRect PersonalBestRect, TeamModeRect;
		RemainingRows.HSplitTop(16.0f, &PersonalBestRect, &TeamModeRect);
		if(GameClient()->m_ReceivedDDNetPlayerFinishTimes && LocalId >= 0 &&
			GameClient()->m_aClients[LocalId].m_FinishTimeSeconds != FinishTime::UNSET &&
			GameClient()->m_aClients[LocalId].m_FinishTimeSeconds != FinishTime::NOT_FINISHED_MILLIS)
		{
			RenderInfoTimeRow(m_pMenus, Graphics(), Ui(), TextRender(), PersonalBestRect, FontIcon::HEART, "Личный рекорд", GameClient()->m_aClients[LocalId].m_FinishTimeSeconds, GameClient()->m_aClients[LocalId].m_FinishTimeMillis, true, 14.0f);
		}
		else
		{
			RenderInfoRows(m_pMenus, Graphics(), Ui(), TextRender(), PersonalBestRect, {{FontIcon::HEART, "Личный рекорд", "--"}}, 14.0f);
		}

		vRows.clear();
		vRows.push_back({FontIcon::USER, "Командный режим", GameClient()->m_GameInfo.m_DDRaceTeam ? (Config()->m_SvTeam == SV_TEAM_MANDATORY ? "Обязательный" : (Config()->m_SvTeam == SV_TEAM_FORCED_SOLO ? "Соло" : "Разрешен")) : "--"});
		RenderInfoRows(m_pMenus, Graphics(), Ui(), TextRender(), TeamModeRect, vRows, 14.0f);
		RenderPreviewMap(m_pMenus, Graphics(), ServerInfo, PreviewRect);
	}

	{
		CUIRect Inner = SessionPanel;
		Inner.Margin(10.0f, &Inner);
		CUIRect HeaderRect, RowsRect;
		Inner.HSplitTop(14.0f, &HeaderRect, &RowsRect);
		RowsRect.HSplitTop(6.0f, nullptr, &RowsRect);

		RenderSectionHeader(m_pMenus, Graphics(), Ui(), TextRender(), HeaderRect, FontIcon::PAUSE, "ТЕКУЩАЯ СЕССИЯ");

		std::vector<SInfoRow> vRows;
		vRows.push_back({FontIcon::PLAY, "Время забега", FormatRaceTime(GameClient()->CurrentRaceTime()), WARNING_COLOR});
		vRows.push_back({FontIcon::FLAG_CHECKERED, "Чекпоинт", FormatCheckpoint(GameClient(), LocalId)});
		vRows.push_back({FontIcon::XMARK, "Смертей", LocalId >= 0 ? std::to_string(GameClient()->m_aStats[LocalId].m_Deaths) : std::string("--")});
		RenderInfoRows(m_pMenus, Graphics(), Ui(), TextRender(), RowsRect, vRows, 14.0f);
	}

	{
		CUIRect Inner = PlayersPanel;
		Inner.Margin(10.0f, &Inner);
		CUIRect HeaderRect, TableRect;
		Inner.HSplitTop(14.0f, &HeaderRect, &TableRect);
		TableRect.HSplitTop(6.0f, nullptr, &TableRect);

		RenderSectionHeader(m_pMenus, Graphics(), Ui(), TextRender(), HeaderRect, FontIcon::USERS, "ИГРОКИ НА СЕРВЕРЕ");

		Graphics()->DrawRect4(TableRect.x, TableRect.y, TableRect.w, TableRect.h,
			ColorRGBA(0.035f, 0.032f, 0.028f, 0.82f), ColorRGBA(0.035f, 0.032f, 0.028f, 0.82f),
			ColorRGBA(0.018f, 0.017f, 0.015f, 0.92f), ColorRGBA(0.018f, 0.017f, 0.015f, 0.92f),
			IGraphics::CORNER_ALL, 9.0f);

		CUIRect HeaderRow;
		TableRect.HSplitTop(14.0f, &HeaderRow, &TableRect);
		Graphics()->DrawRect(HeaderRow.x, HeaderRow.y, HeaderRow.w, HeaderRow.h,
			ColorRGBA(0.11f, 0.10f, 0.09f, 0.92f), IGraphics::CORNER_ALL, 8.0f);

		CUIRect TimeRect, NameRect, ClanRect, PingRect, StatusRect;
		HeaderRow.VSplitLeft(94.0f, &TimeRect, &HeaderRow);
		HeaderRow.VSplitLeft(18.0f, nullptr, &HeaderRow);
		HeaderRow.VSplitRight(82.0f, &HeaderRow, &StatusRect);
		HeaderRow.VSplitRight(48.0f, &HeaderRow, &PingRect);
		HeaderRow.VSplitRight(118.0f, &HeaderRow, &ClanRect);
		NameRect = HeaderRow;

		TextRender()->TextColor(TEXT_MUTED);
		Ui()->DoLabel(&TimeRect, "Время", BODY_TEXT_SMALL, TEXTALIGN_MC);
		Ui()->DoLabel(&NameRect, "Имя игрока", BODY_TEXT_SMALL, TEXTALIGN_ML);
		Ui()->DoLabel(&ClanRect, "Клан", BODY_TEXT_SMALL, TEXTALIGN_MC);
		Ui()->DoLabel(&PingRect, "Пинг", BODY_TEXT_SMALL, TEXTALIGN_MC);
		Ui()->DoLabel(&StatusRect, "Состояние", BODY_TEXT_SMALL, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		std::vector<const CNetObj_PlayerInfo *> vPlayerInfos;
		vPlayerInfos.reserve(MAX_CLIENTS);
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			const CNetObj_PlayerInfo *pInfo = GameClient()->m_Snap.m_apInfoByScore[i];
			if(pInfo != nullptr)
				vPlayerInfos.push_back(pInfo);
		}

		std::stable_sort(vPlayerInfos.begin(), vPlayerInfos.end(), [&](const CNetObj_PlayerInfo *pLeftInfo, const CNetObj_PlayerInfo *pRightInfo) {
			const int Left = pLeftInfo->m_ClientId;
			const int Right = pRightInfo->m_ClientId;
			const int64_t LeftTime = GetClientRaceTimeValue(GameClient(), Left);
			const int64_t RightTime = GetClientRaceTimeValue(GameClient(), Right);
			if(LeftTime == RightTime)
				return str_comp_nocase(GameClient()->m_aClients[Left].m_aName, GameClient()->m_aClients[Right].m_aName) < 0;
			if(LeftTime < 0)
				return false;
			if(RightTime < 0)
				return true;
			return LeftTime < RightTime;
		});

		const float RowHeight = 17.0f;
		static CScrollRegion s_PlayerScrollRegion;
		CScrollRegionParams ScrollParams;
		ScrollParams.m_ScrollUnit = RowHeight * 3.0f;
		ScrollParams.m_ScrollbarThickness = 0.0f;
		ScrollParams.m_ScrollbarMargin = 0.0f;
		ScrollParams.m_SliderMinSize = 0.0f;
		ScrollParams.m_ClipBgColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		ScrollParams.m_ScrollbarBgColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		ScrollParams.m_RailBgColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		ScrollParams.m_SliderColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		ScrollParams.m_SliderColorHover = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		ScrollParams.m_SliderColorGrabbed = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		s_PlayerScrollRegion.Begin(&TableRect, &ScrollParams);
		for(int RowIndex = 0; RowIndex < (int)vPlayerInfos.size(); ++RowIndex)
		{
			const CNetObj_PlayerInfo *pPlayerInfo = vPlayerInfos[RowIndex];
			const int ClientId = pPlayerInfo->m_ClientId;
			const auto &ClientData = GameClient()->m_aClients[ClientId];

			CUIRect RowRect;
			TableRect.HSplitTop(RowHeight - 2.0f, &RowRect, &TableRect);
			TableRect.HSplitTop(2.0f, nullptr, &TableRect);
			if(!s_PlayerScrollRegion.AddRect(RowRect))
				continue;

			const bool IsLocal = ClientId == LocalId;
			const ColorRGBA RowTint = IsLocal ? ColorRGBA(0.64f, 0.47f, 0.21f, 0.24f) : ColorRGBA(0.08f, 0.074f, 0.066f, 0.76f);
			Graphics()->DrawRect(RowRect.x, RowRect.y, RowRect.w, RowRect.h,
				RowTint, IGraphics::CORNER_ALL, 8.0f);

			CUIRect TeeRect;
			RowRect.VSplitLeft(94.0f, &TimeRect, &RowRect);
			RowRect.VSplitLeft(18.0f, &TeeRect, &RowRect);
			RowRect.VSplitRight(82.0f, &RowRect, &StatusRect);
			RowRect.VSplitRight(48.0f, &RowRect, &PingRect);
			RowRect.VSplitRight(118.0f, &RowRect, &ClanRect);
			NameRect = RowRect;

			TextRender()->TextColor(IsLocal ? TEXT_MAIN : TEXT_DIM);
			if(!RenderClientRaceTime(GameClient(), Ui(), TimeRect, BODY_TEXT_SMALL, ClientId))
				Ui()->DoLabel(&TimeRect, "--", BODY_TEXT_SMALL, TEXTALIGN_MC);

			CUIRect TeeInner = TeeRect;
			TeeInner.Margin(2.0f, &TeeInner);
			RenderClientPreview(RenderTools(), TeeInner, ClientData);

			Ui()->DoLabel(&NameRect, ClientData.m_aName, BODY_TEXT_SMALL, TEXTALIGN_ML);
			Ui()->DoLabel(&ClanRect, ClientData.m_aClan[0] ? ClientData.m_aClan : "-", BODY_TEXT_SMALL, TEXTALIGN_MC);

			const int DisplayPing = GetDisplayPing(GameClient(), pPlayerInfo, ClientId, LocalId);
			const ColorRGBA PingColor = DisplayPing >= 0 ? (DisplayPing < 80 ? SUCCESS_COLOR : (DisplayPing < 140 ? WARNING_COLOR : DANGER_COLOR)) : TEXT_MUTED;
			TextRender()->TextColor(PingColor);
			if(DisplayPing >= 0)
			{
				char aPing[16];
				str_format(aPing, sizeof(aPing), "%d", DisplayPing);
				Ui()->DoLabel(&PingRect, aPing, BODY_TEXT_SMALL, TEXTALIGN_MC);
			}
			else
			{
				Ui()->DoLabel(&PingRect, "--", BODY_TEXT_SMALL, TEXTALIGN_MC);
			}

			const char *pStatus = ClientData.m_Spec ? "Наблюдает" : (ClientData.m_Afk ? "AFK" : (ClientData.m_Paused ? "В меню" : "Играет"));
			const ColorRGBA StatusColor = ClientData.m_Spec || ClientData.m_Paused ? WARNING_COLOR : (ClientData.m_Afk ? DANGER_COLOR : SUCCESS_COLOR);
			TextRender()->TextColor(StatusColor);
			Ui()->DoLabel(&StatusRect, pStatus, BODY_TEXT_SMALL, TEXTALIGN_MC);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		s_PlayerScrollRegion.End();
	}

	{
		CUIRect Inner = ActionsColumn;
		Inner.Margin(10.0f, &Inner);
		CUIRect HeaderRect, ButtonsRect;
		Inner.HSplitTop(14.0f, &HeaderRect, &ButtonsRect);
		ButtonsRect.HSplitTop(6.0f, nullptr, &ButtonsRect);

		RenderSectionHeader(m_pMenus, Graphics(), Ui(), TextRender(), HeaderRect, FontIcon::STAR, "БЫСТРЫЕ ДЕЙСТВИЯ");

		const float ButtonHeight = 30.0f;
		const float ButtonSpacing = 5.0f;
		CUIRect ButtonRect;

		static CButtonContainer s_ContinueButton;
		ButtonsRect.HSplitTop(ButtonHeight, &ButtonRect, &ButtonsRect);
		const bool CanTogglePause = GameClient()->m_ReceivedDDNetPlayer && HasLocalInfo && (GameClient()->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS || Paused || Spec);
		const char *pPrimaryLabel = (!Paused && !Spec) ? "Продолжить" : "В игру";
		if(DoActionButton(m_pMenus, Graphics(), Ui(), TextRender(), ButtonRect, &s_ContinueButton, FontIcon::PLAY, pPrimaryLabel, "", EActionButtonStyle::PRIMARY, !Paused && !Spec ? true : CanTogglePause))
		{
			if(!Paused && !Spec)
				m_pMenus->SetActive(false);
			else
			{
				Console()->ExecuteLine("say /pause", IConsole::CLIENT_ID_UNSPECIFIED);
				m_pMenus->SetActive(false);
			}
		}

		ButtonsRect.HSplitTop(ButtonSpacing, nullptr, &ButtonsRect);
		static CButtonContainer s_KillButton;
		const bool CanKill = LocalId >= 0 && HasLocalInfo && GameClient()->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS && !Paused && !Spec;
		ButtonsRect.HSplitTop(ButtonHeight, &ButtonRect, &ButtonsRect);
		if(DoActionButton(m_pMenus, Graphics(), Ui(), TextRender(), ButtonRect, &s_KillButton, FontIcon::ARROWS_ROTATE, "Респаун", "", EActionButtonStyle::NORMAL, CanKill))
		{
			GameClient()->SendKill();
			m_pMenus->SetActive(false);
		}

		ButtonsRect.HSplitTop(ButtonSpacing, nullptr, &ButtonsRect);
		static CButtonContainer s_SpectateButton;
		const bool CanSpectate = LocalId >= 0 && HasLocalInfo && GameClient()->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS && !Paused && !Spec && !Client()->DummyConnecting() && (g_Config.m_ClDummy == 0 || Client()->DummyConnected());
		ButtonsRect.HSplitTop(ButtonHeight, &ButtonRect, &ButtonsRect);
		if(DoActionButton(m_pMenus, Graphics(), Ui(), TextRender(), ButtonRect, &s_SpectateButton, FontIcon::EYE, "Наблюдать", "", EActionButtonStyle::NORMAL, CanSpectate))
		{
			GameClient()->SendSwitchTeam(TEAM_SPECTATORS);
			m_pMenus->SetActive(false);
		}

		ButtonsRect.HSplitTop(ButtonSpacing, nullptr, &ButtonsRect);
		static CButtonContainer s_SettingsButton;
		ButtonsRect.HSplitTop(ButtonHeight, &ButtonRect, &ButtonsRect);
		if(DoActionButton(m_pMenus, Graphics(), Ui(), TextRender(), ButtonRect, &s_SettingsButton, FontIcon::GEAR, "Настройки", "", EActionButtonStyle::NORMAL, true))
			m_pMenus->m_GamePage = CMenus::PAGE_SETTINGS;

		ButtonsRect.HSplitTop(ButtonSpacing, nullptr, &ButtonsRect);
		static CButtonContainer s_DemoButton;
		const bool Recording = DemoRecorder(RECORDER_MANUAL)->IsRecording();
		ButtonsRect.HSplitTop(ButtonHeight, &ButtonRect, &ButtonsRect);
		if(DoActionButton(m_pMenus, Graphics(), Ui(), TextRender(), ButtonRect, &s_DemoButton, FontIcon::CIRCLE, Recording ? "Остановить демо" : "Записать демо", "", EActionButtonStyle::NORMAL, true))
		{
			if(!Recording)
				Client()->DemoRecorder_Start(GameClient()->Map()->BaseName(), true, RECORDER_MANUAL);
			else
				Client()->DemoRecorder(RECORDER_MANUAL)->Stop(IDemoRecorder::EStopMode::KEEP_FILE);
		}

		ButtonsRect.HSplitTop(ButtonSpacing, nullptr, &ButtonsRect);
		static CButtonContainer s_DummyButton;
		const bool DummyEnabled = Client()->DummyAllowed() && !Client()->DummyConnectingDelayed() && !Client()->DummyConnecting();
		const char *pDummyLabel = Client()->DummyConnected() ? "Отключить дамми" : "Подключить дамми";
		if(!Client()->DummyAllowed())
			pDummyLabel = "Дамми недоступен";
		else if(Client()->DummyConnectingDelayed())
			pDummyLabel = "Пожалуйста, подождите";
		else if(Client()->DummyConnecting())
			pDummyLabel = "Подключение дамми";
		ButtonsRect.HSplitTop(ButtonHeight, &ButtonRect, &ButtonsRect);
		if(DoActionButton(m_pMenus, Graphics(), Ui(), TextRender(), ButtonRect, &s_DummyButton, FontIcon::USER, pDummyLabel, "", EActionButtonStyle::NORMAL, DummyEnabled))
		{
			if(!Client()->DummyConnected())
				Client()->DummyConnect();
			else if(GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmDisconnectTime && g_Config.m_ClConfirmDisconnectTime >= 0)
			{
				m_pMenus->PopupConfirm(Localize("Disconnect Dummy"), Localize("Are you sure that you want to disconnect your dummy?"), Localize("Yes"), Localize("No"), &CMenus::PopupConfirmDisconnectDummy);
			}
			else
			{
				Client()->DummyDisconnect(nullptr);
				m_pMenus->SetActive(false);
			}
		}

		ButtonsRect.HSplitTop(ButtonSpacing, nullptr, &ButtonsRect);
		static CButtonContainer s_DisconnectButton;
		ButtonsRect.HSplitTop(ButtonHeight, &ButtonRect, &ButtonsRect);
		if(DoActionButton(m_pMenus, Graphics(), Ui(), TextRender(), ButtonRect, &s_DisconnectButton, FontIcon::POWER_OFF, "Отключиться", "", EActionButtonStyle::DANGER, true))
		{
			if(GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmDisconnectTime && g_Config.m_ClConfirmDisconnectTime >= 0)
			{
				m_pMenus->PopupConfirm(Localize("Disconnect"), Localize("Are you sure that you want to disconnect?"), Localize("Yes"), Localize("No"), &CMenus::PopupConfirmDisconnect);
			}
			else
			{
				Client()->Disconnect();
				m_pMenus->RefreshBrowserTab(true);
			}
		}
	}

	/*{
		CUIRect FooterPill = Footer;
		FooterPill.VMargin(Footer.w * 0.20f, &FooterPill);
		FooterPill.HMargin(4.0f, &FooterPill);
		Graphics()->DrawRectGlow(FooterPill.x, FooterPill.y, FooterPill.w, FooterPill.h, PANEL_GLOW.WithAlpha(0.14f), 14.0f, IGraphics::CORNER_ALL, 14.0f, 4);
		Graphics()->DrawRect4(FooterPill.x, FooterPill.y, FooterPill.w, FooterPill.h,
			ColorRGBA(0.10f, 0.09f, 0.07f, 0.66f), ColorRGBA(0.10f, 0.09f, 0.07f, 0.66f),
			ColorRGBA(0.06f, 0.05f, 0.04f, 0.78f), ColorRGBA(0.06f, 0.05f, 0.04f, 0.78f),
			IGraphics::CORNER_ALL, 14.0f);
		Graphics()->DrawRectOutline(FooterPill.x, FooterPill.y, FooterPill.w, FooterPill.h, PANEL_BORDER.WithAlpha(0.34f), IGraphics::CORNER_ALL, IGraphics::SIDE_ALL, 14.0f);
		TextRender()->TextColor(TEXT_DIM);
		Ui()->DoLabel(&FooterPill, "Esc — продолжить    •    F1 — помощь    •    F10 — скриншот", BODY_TEXT_SMALL, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}*/
}
