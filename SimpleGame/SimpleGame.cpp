/*
    Quarter-view 2.5D tutorial prototype
    Controls: WASD move, E interact, F attack, SPACE dodge, R restart, ESC quit
*/

#include "stdafx.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"
#include "Renderer.h"

struct Vec2
{
	float x;
	float y;
	Vec2(float px = 0.0f, float py = 0.0f) : x(px), y(py) {}
};

struct Npc
{
	Vec2 position;
	float r, g, b;
	std::wstring name;
	std::wstring line;
	float animationOffset;
};

struct NpcWander
{
	Vec2 home;
	Vec2 target;
	float timer;
	float phase;
	bool moving;
};
std::vector<NpcWander> g_NpcWander;

struct Monster
{
	Vec2 position;
	int health;
	bool alive;
};

struct Animal
{
	enum Kind { DEER, BOAR, WOLF };
	Kind kind;
	Vec2 position;
	Vec2 home;
	Vec2 direction;
	float changeTimer;
	float animationOffset;
	int health;
	bool alive;
};

struct WorldObject
{
	enum Type { HOUSE, TREE, LANTERN, NPC_OBJECT, PLAYER_OBJECT, MONSTER_OBJECT, ANIMAL_OBJECT, BOY_OBJECT, CLUE_OBJECT };
	Type type;
	Vec2 position;
	int index;
	float depth;
};

enum QuestState
{
	MEET_ELIA,
	FIND_BOOT,
	FIND_REEDS,
	DEFEAT_CREATURES,
	FIND_BOY,
	RETURN_TO_ELIA,
	QUEST_COMPLETE
};

Renderer* g_Renderer = NULL;
int g_WindowWidth = 1000;
int g_WindowHeight = 700;
bool g_Keys[256] = { false };
HFONT g_KoreanFont = NULL;
bool g_FontReady = false;
std::map<wchar_t, GLuint> g_FontGlyphs;

Vec2 g_Player(-5.5f, -4.5f);
Vec2 g_Facing(0.7f, 0.7f);
int g_PlayerHealth = 4;
float g_HitCooldown = 0.0f;
float g_AttackFlash = 0.0f;
float g_DodgeCooldown = 0.0f;
float g_GameTime = 0.0f;
int g_LastUpdateMs = 0;
bool g_PlayerMoving = false;

QuestState g_Quest = MEET_ELIA;
std::wstring g_Dialogue = L"등불을 따라 회암 마을로 들어가십시오.";
std::wstring g_Speaker = L"안내";
float g_DialogueTimer = 5.0f;

const Vec2 kEliaPosition(-0.5f, -1.0f);
const Vec2 kBootPosition(4.2f, 3.6f);
const Vec2 kReedsPosition(7.0f, 6.3f);
Vec2 g_BoyPosition(9.0f, 7.9f);

std::vector<Npc> g_Npcs;
std::vector<Vec2> g_Trees;
std::vector<Vec2> g_Houses;
std::vector<Vec2> g_Lanterns;
std::vector<Monster> g_Monsters;
std::vector<Animal> g_Animals;

float Distance(const Vec2& a, const Vec2& b)
{
	const float dx = a.x - b.x;
	const float dy = a.y - b.y;
	return std::sqrt(dx * dx + dy * dy);
}

Vec2 Normalize(const Vec2& v)
{
	const float length = std::sqrt(v.x * v.x + v.y * v.y);
	if (length < 0.001f)
		return Vec2();
	return Vec2(v.x / length, v.y / length);
}

Vec2 ProjectRaw(const Vec2& world)
{
	return Vec2((world.x - world.y) * 27.0f,
		-(world.x + world.y) * 14.0f);
}

Vec2 Project(const Vec2& world)
{
	const Vec2 point = ProjectRaw(world);
	const Vec2 camera = ProjectRaw(g_Player);
	return Vec2(point.x - camera.x, point.y - camera.y - 35.0f);
}

bool IsVisible(const Vec2& screen, float margin = 140.0f)
{
	return screen.x > -g_WindowWidth * 0.5f - margin &&
		screen.x < g_WindowWidth * 0.5f + margin &&
		screen.y > -g_WindowHeight * 0.5f - margin &&
		screen.y < g_WindowHeight * 0.5f + margin;
}

void ShowDialogue(const std::wstring& speaker, const std::wstring& text, float duration = 4.0f)
{
	g_Speaker = speaker;
	g_Dialogue = text;
	g_DialogueTimer = duration;
}

void ResetGame()
{
	g_Player = Vec2(-5.5f, -4.5f);
	g_Facing = Vec2(0.7f, 0.7f);
	g_PlayerHealth = 4;
	g_HitCooldown = 0.0f;
	g_AttackFlash = 0.0f;
	g_DodgeCooldown = 0.0f;
	g_GameTime = 0.0f;
	g_PlayerMoving = false;
	for (size_t i=0; i<g_NpcWander.size(); ++i) {
		g_Npcs[i].position=g_NpcWander[i].home;
		g_NpcWander[i].target=g_NpcWander[i].home;
		g_NpcWander[i].timer=0.5f+float(i%7)*0.4f;
		g_NpcWander[i].phase=0;
		g_NpcWander[i].moving=false;
	}
	g_Quest = MEET_ELIA;
	g_BoyPosition = Vec2(9.0f, 7.9f);
	g_Monsters.clear();
	for (size_t i = 0; i < g_Animals.size(); ++i)
	{
		g_Animals[i].position = g_Animals[i].home;
		g_Animals[i].alive = true;
		g_Animals[i].health = g_Animals[i].kind == Animal::BOAR ? 4 : (g_Animals[i].kind == Animal::WOLF ? 3 : 2);
	}
	ShowDialogue(L"조작 안내", L"WASD 이동  |  E 상호작용  |  F 공격  |  SPACE 회피", 6.0f);
}


struct RoadSegment { Vec2 a, b; };
const RoadSegment kRoads[] = {
	{ Vec2(-22,-1), Vec2(4,-1) }, { Vec2(-5,-18), Vec2(-5,16) },
	{ Vec2(-17,-10), Vec2(-17,11) }, { Vec2(-22,10), Vec2(1,10) },
	{ Vec2(-18,-10), Vec2(1,-10) }, { Vec2(0,-1), Vec2(7,6) },
	{ Vec2(7,6), Vec2(9,7.2f) }
};
float RoadDistance(const Vec2& p) {
	float nearest = 1000.0f;
	for (const RoadSegment& road : kRoads) {
		float dx=road.b.x-road.a.x, dy=road.b.y-road.a.y;
		float t=((p.x-road.a.x)*dx+(p.y-road.a.y)*dy)/(dx*dx+dy*dy);
		t = t < 0 ? 0 : (t > 1 ? 1 : t);
		float d=Distance(p,Vec2(road.a.x+t*dx,road.a.y+t*dy));
		if(d<nearest) nearest=d;
	}
	return nearest;
}

void InitializeWorld()
{
	g_Npcs.clear();
	g_Npcs.push_back({ Vec2(-4.5f, -3.0f), 0.38f, 0.48f, 0.56f, L"경비병 토마스", L"민원은 촌장에게, 괴물은 나에게. 세금은... 더 무서운 곳에 내시오.", 0.1f });
	g_Npcs.push_back({ Vec2(-2.5f, 0.4f), 0.55f, 0.30f, 0.20f, L"대장장이", L"호수 안개는 좋은 쇠를 녹슬게 하지. 쇠를 낭비하는 건 뭐든 싫어.", 0.7f });
	g_Npcs.push_back({ Vec2(-4.0f, 1.6f), 0.44f, 0.30f, 0.18f, L"잡화상", L"세상이 끝난다고? 그럼 등불을 두 개 사시오. 둘 다 필요할 테니.", 1.3f });
	g_Npcs.push_back({ Vec2(1.4f, -2.0f), 0.50f, 0.46f, 0.34f, L"노인 로완", L"옛날엔 호수에 종이 있었지. 아니, 종 안에 호수가 있었나.", 1.9f });
	g_Npcs.push_back({ Vec2(2.0f, 0.8f), 0.34f, 0.48f, 0.36f, L"약초상", L"검은 갈대는 약초가 아니야. 바구니를 물거든.", 2.4f });
	g_Npcs.push_back({ Vec2(-3.0f, -2.0f), 0.48f, 0.38f, 0.52f, L"주민", L"따뜻한 불이 있다는 건 오늘이 아직 우리 것이라는 뜻이지.", 2.9f });
	g_Npcs.push_back({ Vec2(-1.0f, 1.8f), 0.42f, 0.42f, 0.50f, L"방앗간 주인", L"밀가루가 좀 회색이지만 빵에서는 귀신 맛이 아주 조금만 나요.", 3.4f });
	g_Npcs.push_back({ Vec2(1.2f, 2.0f), 0.52f, 0.38f, 0.28f, L"어부", L"일주일째 입질이 없어. 대신 누가 낚싯바늘을 자꾸 돌려줘.", 4.1f });
	g_Npcs.push_back({ Vec2(-5.8f, 0.3f), 0.38f, 0.44f, 0.30f, L"농부", L"봄은 올 거요. 늘 그렇게 고집을 부렸으니까.", 4.7f });
	g_Npcs.push_back({ Vec2(-7.0f, -0.4f), 0.45f, 0.35f, 0.25f, L"목수", L"집은 삐걱거려도 괜찮소. 대답만 하지 않으면 돼.", 5.2f });
	g_Npcs.push_back({ Vec2(-5.5f, 2.5f), 0.32f, 0.38f, 0.50f, L"순찰병", L"밤에는 숲에 가지 마시오. 낮에도 별로 권하고 싶진 않지만.", 5.8f });
	g_Npcs.push_back({ Vec2(-2.0f, -3.6f), 0.50f, 0.42f, 0.30f, L"빵집 주인", L"오늘 빵은 안 탔어요. 연기 나는 건 굴뚝 쪽일 겁니다. 아마도.", 6.3f });
	g_Npcs.push_back({ Vec2(0.8f, -3.7f), 0.38f, 0.30f, 0.46f, L"재봉사", L"검은 실이 밤마다 길어져요. 잘라 놓으면 삐친 것처럼 꼬이고요.", 6.8f });
	g_Npcs.push_back({ Vec2(2.8f, -0.8f), 0.35f, 0.45f, 0.42f, L"수도사", L"두려움은 죄가 아닙니다. 두려움 때문에 남을 버리는 것이 죄이지요.", 7.4f });
	g_Npcs.push_back({ Vec2(3.5f, 1.8f), 0.55f, 0.38f, 0.24f, L"나무꾼", L"숲의 나무를 세어 봤소. 돌아올 때마다 한 그루씩 늘더군.", 7.9f });
	g_Npcs.push_back({ Vec2(-7.5f, -3.5f), 0.40f, 0.48f, 0.33f, L"양치기", L"양 세 마리가 없어졌고 네 마리가 돌아왔소. 어느 놈이 남의 양인지 원.", 8.5f });
	g_Npcs.push_back({ Vec2(-0.2f, 3.0f), 0.48f, 0.34f, 0.40f, L"주점 주인", L"종말이 오기 전에 외상값부터 갚으시오. 순서라는 게 있으니까.", 9.0f });
	g_Npcs.push_back({ Vec2(3.8f, -2.8f), 0.38f, 0.42f, 0.52f, L"여행자", L"이 마을 지도 말인데, 호수가 어제보다 가까워진 것 같소.", 9.5f });
	g_Npcs.push_back({ Vec2(-6.2f, 4.2f), 0.48f, 0.42f, 0.34f, L"석공", L"성당 돌에는 글자가 있어. 해가 지면 안쪽으로 파고들지만.", 10.1f });
	g_Npcs.push_back({ Vec2(4.2f, 0.2f), 0.52f, 0.44f, 0.30f, L"우편 배달부", L"받는 이가 백 년 전에 죽었어도 편지는 배달해야지.", 10.7f });
	g_Npcs.push_back({ Vec2(-3.8f, 4.6f), 0.36f, 0.48f, 0.45f, L"치료사", L"상처는 꿰맬 수 있어요. 기억은 아직 방법을 찾는 중이고요.", 11.2f });
	g_Npcs.push_back({ Vec2(5.0f, 2.8f), 0.46f, 0.36f, 0.28f, L"사냥꾼", L"늑대는 눈을 마주치면 달아나지. 요즘 것들은 웃더군.", 11.8f });
	g_Npcs.push_back({ Vec2(-8.0f, 1.8f), 0.42f, 0.32f, 0.48f, L"견습 학자", L"금서가 아니에요. 도서관이 지나치게 소심한 겁니다.", 12.4f });
	g_Npcs.push_back({ Vec2(1.8f, -4.8f), 0.45f, 0.48f, 0.32f, L"우물지기", L"동전은 던지지 마시오. 밑에서 자꾸 거슬러 주니까.", 12.9f });


	g_Houses = { Vec2(-21,-4),Vec2(-13,-4),Vec2(-8,-5),Vec2(-1,-5),
		Vec2(-21,3),Vec2(-12,3),Vec2(-8,6),Vec2(0,4),
		Vec2(-21,14),Vec2(-12,14),Vec2(-1,14),Vec2(-12,-14) };
	g_Lanterns = { Vec2(-5,-3),Vec2(-1,-1),Vec2(-10,-1),Vec2(-18,-1),
		Vec2(-5,7),Vec2(-15,10),Vec2(-5,-10),Vec2(-17,-8),
		Vec2(3,2),Vec2(6,5),Vec2(8,7) };
	// Keep quest actors near the starting square; distribute residents along streets.
	for (size_t i=0; i<g_Npcs.size(); ++i) {
		const RoadSegment& road=kRoads[i % 5];
		float t=0.14f + float(i/5)*0.16f;
		g_Npcs[i].position=Vec2(road.a.x+(road.b.x-road.a.x)*t,
			road.a.y+(road.b.y-road.a.y)*t);
	}



	g_NpcWander.clear();
	for(size_t i=0;i<g_Npcs.size();++i)
		g_NpcWander.push_back({g_Npcs[i].position,g_Npcs[i].position,1.0f,0.0f,false});

	g_Trees.clear();
	// Distributed stands with road clearances and open gaps, not a central ring.
	for(int y=-24; y<=32; y+=2) for(int x=-27; x<=31; x+=2) {
		unsigned int h=static_cast<unsigned int>((x+40)*73856093u)^
			static_cast<unsigned int>((y+40)*19349663u);
		if(h%100>65) continue;
		Vec2 p(x+float(h%13)*0.09f,y+float((h>>8)%11)*0.09f);
		if(RoadDistance(p)<2.0f || Distance(p,Vec2(10.4f,8.8f))<5.5f) continue;
		bool village=p.x>-24 && p.x<3 && p.y>-17 && p.y<17;
		if(village && h%11!=0) continue;
		bool blocked=false;
		for(const Vec2& house:g_Houses) if(Distance(p,house)<3.0f) blocked=true;
		for(const Npc& npc:g_Npcs) if(Distance(p,npc.position)<1.4f) blocked=true;
		if(!blocked) g_Trees.push_back(p);
	}

	g_Animals.clear();
	g_Animals.push_back({ Animal::DEER, Vec2(16.0f, 5.0f), Vec2(16.0f, 5.0f), Vec2(1.0f, 0.2f), 2.0f, 0.3f, 2, true });
	g_Animals.push_back({ Animal::DEER, Vec2(19.0f, 7.0f), Vec2(19.0f, 7.0f), Vec2(-0.4f, 0.8f), 1.2f, 1.1f, 2, true });
	g_Animals.push_back({ Animal::DEER, Vec2(-15.0f, 12.0f), Vec2(-15.0f, 12.0f), Vec2(0.3f, -1.0f), 2.8f, 2.0f, 2, true });
	g_Animals.push_back({ Animal::DEER, Vec2(-20.0f, -12.0f), Vec2(-20.0f, -12.0f), Vec2(0.8f, 0.5f), 3.1f, 2.8f, 2, true });
	g_Animals.push_back({ Animal::BOAR, Vec2(13.0f, -8.0f), Vec2(13.0f, -8.0f), Vec2(-0.6f, 0.3f), 1.7f, 3.4f, 4, true });
	g_Animals.push_back({ Animal::BOAR, Vec2(20.0f, -5.0f), Vec2(20.0f, -5.0f), Vec2(0.4f, 0.7f), 2.3f, 4.2f, 4, true });
	g_Animals.push_back({ Animal::BOAR, Vec2(-18.0f, 20.0f), Vec2(-18.0f, 20.0f), Vec2(0.7f, -0.2f), 2.6f, 5.1f, 4, true });
	g_Animals.push_back({ Animal::WOLF, Vec2(22.0f, 16.0f), Vec2(22.0f, 16.0f), Vec2(-0.8f, -0.2f), 1.0f, 5.8f, 3, true });
	g_Animals.push_back({ Animal::WOLF, Vec2(25.0f, 14.0f), Vec2(25.0f, 14.0f), Vec2(0.3f, -0.7f), 1.9f, 6.5f, 3, true });
	g_Animals.push_back({ Animal::WOLF, Vec2(-22.0f, 5.0f), Vec2(-22.0f, 5.0f), Vec2(0.6f, 0.4f), 2.2f, 7.2f, 3, true });
	ResetGame();
}

bool InsideLake(const Vec2& point)
{
	const float dx = (point.x - 10.4f) / 3.5f;
	const float dy = (point.y - 8.8f) / 2.6f;
	return dx * dx + dy * dy < 0.72f;
}

bool IsWalkable(const Vec2& position)
{
	if (position.x < -28.0f || position.x > 32.0f || position.y < -25.0f || position.y > 33.0f)
		return false;
	if (InsideLake(position))
		return false;

	for (size_t i = 0; i < g_Houses.size(); ++i)
	{
		if (std::fabs(position.x - g_Houses[i].x) < 1.15f &&
			std::fabs(position.y - g_Houses[i].y) < 0.95f)
			return false;
	}
	for (size_t i = 0; i < g_Trees.size(); ++i)
	{
		if (Distance(position, g_Trees[i]) < 0.46f)
			return false;
	}
	return true;
}

void TryMove(const Vec2& delta)
{
	Vec2 nextX(g_Player.x + delta.x, g_Player.y);
	if (IsWalkable(nextX))
		g_Player.x = nextX.x;
	Vec2 nextY(g_Player.x, g_Player.y + delta.y);
	if (IsWalkable(nextY))
		g_Player.y = nextY.y;
}

void Interact()
{
	if (Distance(g_Player, kEliaPosition) < 1.35f)
	{
		if (g_Quest == MEET_ELIA)
		{
			g_Quest = FIND_BOOT;
			ShowDialogue(L"엘리아", L"동생이 호수에서 돌아가신 어머니의 목소리를 들었대요. 제발 찾아주세요.", 5.0f);
		}
		else if (g_Quest == RETURN_TO_ELIA)
		{
			g_Quest = QUEST_COMPLETE;
			g_BoyPosition = Vec2(-0.1f, -1.6f);
			ShowDialogue(L"엘리아", L"당신은 작은 생명 하나를 구했어요. 하지만 제게는 그 아이가 세상 전부예요.", 7.0f);
		}
		else
		{
			ShowDialogue(L"엘리아", L"아렌을 찾아주세요. 동쪽 길이 숲과 호수로 이어져요.");
		}
		return;
	}

	if (g_Quest == FIND_BOOT && Distance(g_Player, kBootPosition) < 1.1f)
	{
		g_Quest = FIND_REEDS;
		ShowDialogue(L"단서", L"흠뻑 젖은 아이의 장화다. 젖은 발자국이 호수 쪽으로 이어진다.");
		return;
	}

	if (g_Quest == FIND_REEDS && Distance(g_Player, kReedsPosition) < 1.1f)
	{
		g_Quest = DEFEAT_CREATURES;
		g_Monsters.clear();
		g_Monsters.push_back({ Vec2(7.8f, 6.9f), 2, true });
		g_Monsters.push_back({ Vec2(6.5f, 7.5f), 2, true });
		ShowDialogue(L"검은 갈대", L"갈대가 당신의 이름을 속삭인다. 안개 속에서 무언가 기어 나온다.");
		return;
	}

	if (g_Quest == FIND_BOY && Distance(g_Player, g_BoyPosition) < 1.35f)
	{
		g_Quest = RETURN_TO_ELIA;
		ShowDialogue(L"아렌", L"엄마가 호수는 우리를 기억한다고 했어요. 이제 당신도 알아본 것 같아요.", 6.0f);
		return;
	}

	for (size_t i = 0; i < g_Npcs.size(); ++i)
	{
		if (Distance(g_Player, g_Npcs[i].position) < 1.1f)
		{
			ShowDialogue(g_Npcs[i].name, g_Npcs[i].line);
			return;
		}
	}

	ShowDialogue(L"안내", L"주변에 상호작용할 대상이 없습니다.", 1.5f);
}

void Attack()
{
	if (g_AttackFlash > 0.0f)
		return;
	g_AttackFlash = 0.22f;

	for (size_t i = 0; i < g_Monsters.size(); ++i)
	{
		Monster& monster = g_Monsters[i];
		if (!monster.alive)
			continue;
		const Vec2 toward(monster.position.x - g_Player.x, monster.position.y - g_Player.y);
		const float facingDot = toward.x * g_Facing.x + toward.y * g_Facing.y;
		if (Distance(g_Player, monster.position) < 1.35f && facingDot > -0.2f)
		{
			monster.health--;
			if (monster.health <= 0)
				monster.alive = false;
		}
	}

	for (size_t i = 0; i < g_Animals.size(); ++i)
	{
		Animal& animal = g_Animals[i];
		if (!animal.alive) continue;
		const Vec2 toward(animal.position.x - g_Player.x, animal.position.y - g_Player.y);
		const float facingDot = toward.x * g_Facing.x + toward.y * g_Facing.y;
		if (Distance(g_Player, animal.position) < 1.4f && facingDot > -0.2f)
		{
			animal.health--;
			animal.direction = Normalize(toward);
			if (animal.health <= 0) animal.alive = false;
		}
	}
}

void Dodge()
{
	if (g_DodgeCooldown > 0.0f)
		return;
	g_DodgeCooldown = 0.8f;
	TryMove(Vec2(g_Facing.x * 1.1f, g_Facing.y * 1.1f));
}

void UpdateMonsters(float deltaTime)
{
	if (g_Quest != DEFEAT_CREATURES)
		return;

	int aliveCount = 0;
	for (size_t i = 0; i < g_Monsters.size(); ++i)
	{
		Monster& monster = g_Monsters[i];
		if (!monster.alive)
			continue;
		aliveCount++;
		Vec2 direction = Normalize(Vec2(g_Player.x - monster.position.x, g_Player.y - monster.position.y));
		monster.position.x += direction.x * 0.75f * deltaTime;
		monster.position.y += direction.y * 0.75f * deltaTime;

		if (Distance(g_Player, monster.position) < 0.65f && g_HitCooldown <= 0.0f)
		{
			g_PlayerHealth--;
			g_HitCooldown = 1.2f;
			ShowDialogue(L"전투 안내", L"공격받았습니다. 적이 다가올 때 SPACE로 회피하십시오.", 2.5f);
			if (g_PlayerHealth <= 0)
			{
				g_PlayerHealth = 4;
				g_Player = Vec2(5.8f, 5.2f);
				ShowDialogue(L"안내", L"마을의 등불이 당신을 어둠 속에서 끌어냈습니다.", 3.0f);
			}
		}
	}

	if (aliveCount == 0)
	{
		g_Quest = FIND_BOY;
		ShowDialogue(L"안내", L"괴물들이 호숫물로 녹아내렸습니다. 실종된 소년을 찾으십시오.", 4.0f);
	}
}

void UpdateAnimals(float deltaTime)
{
	for (size_t i = 0; i < g_Animals.size(); ++i)
	{
		Animal& animal = g_Animals[i];
		if (!animal.alive) continue;
		const float playerDistance = Distance(g_Player, animal.position);
		animal.changeTimer -= deltaTime;

		if (animal.kind == Animal::DEER && playerDistance < 5.0f)
			animal.direction = Normalize(Vec2(animal.position.x - g_Player.x, animal.position.y - g_Player.y));
		else if (animal.kind == Animal::WOLF && playerDistance < 6.5f)
		{
			animal.direction = Normalize(Vec2(g_Player.x - animal.position.x, g_Player.y - animal.position.y));
			if (playerDistance < 0.75f && g_HitCooldown <= 0.0f)
			{
				g_PlayerHealth--;
				g_HitCooldown = 1.2f;
				ShowDialogue(L"전투 안내", L"늑대에게 공격받았습니다. 회피 후 반격하십시오.", 2.0f);
			}
		}
		else if (animal.changeTimer <= 0.0f)
		{
			const float seed = g_GameTime * 0.31f + animal.animationOffset * 2.7f;
			animal.direction = Normalize(Vec2(std::sin(seed), std::cos(seed * 1.37f)));
			animal.changeTimer = 2.0f + std::fmod(animal.animationOffset, 2.5f);
		}

		const float speed = animal.kind == Animal::DEER ? 1.35f : (animal.kind == Animal::WOLF ? 1.1f : 0.62f);
		Vec2 next(animal.position.x + animal.direction.x * speed * deltaTime,
			animal.position.y + animal.direction.y * speed * deltaTime);
		if (Distance(next, animal.home) > 8.0f && playerDistance >= 5.0f)
			animal.direction = Normalize(Vec2(animal.home.x - animal.position.x, animal.home.y - animal.position.y));
		else if (IsWalkable(next))
			animal.position = next;
		else
			animal.direction = Vec2(-animal.direction.y, animal.direction.x);
	}
}


bool NpcStepClear(const Vec2& p, size_t self)
{
	if (!IsWalkable(p) || Distance(p,kEliaPosition)<0.85f ||
		Distance(p,g_BoyPosition)<0.65f || Distance(p,g_Player)<0.65f)
		return false;
	for(size_t j=0;j<g_Npcs.size();++j)
		if(j!=self && Distance(p,g_Npcs[j].position)<0.6f) return false;
	return true;
}

void UpdateNpcWander(float dt)
{
	// Quest actors are separate from g_Npcs and never enter this update.
	for(size_t i=0;i<g_Npcs.size();++i) {
		Npc& npc=g_Npcs[i];
		NpcWander& state=g_NpcWander[i];
		if(g_DialogueTimer>0 && g_Speaker==npc.name) {
			state.moving=false;
			state.timer=1.0f;
			continue;
		}
		if(!state.moving) {
			state.timer-=dt;
			if(state.timer>0) continue;
			bool chosen=false;
			for(int attempt=0;attempt<12;++attempt) {
				float angle=g_GameTime*0.47f+float(i)*2.399f+attempt*1.17f;
				float radius=0.7f+float((i+attempt)%5)*0.25f;
				Vec2 goal(state.home.x+std::cos(angle)*radius,
					state.home.y+std::sin(angle)*radius);
				bool clear=NpcStepClear(goal,i);
				// Check the whole short path, not only its endpoint.
				for(int k=1;k<=10 && clear;++k) {
					float t=k/10.0f;
					Vec2 p(npc.position.x+(goal.x-npc.position.x)*t,
						npc.position.y+(goal.y-npc.position.y)*t);
					clear=NpcStepClear(p,i);
				}
				if(clear && Distance(goal,npc.position)>0.35f) {
					state.target=goal; state.moving=true; chosen=true; break;
				}
			}
			if(!chosen) state.timer=1.0f;
			continue;
		}
		float distance=Distance(state.target,npc.position);
		float step=(0.50f+float(i%4)*0.07f)*dt;
		if(distance<0.05f) {
			state.moving=false; state.timer=1.5f+float(i%5)*0.5f; continue;
		}
		if(step>distance) step=distance;
		Vec2 direction=Normalize(Vec2(state.target.x-npc.position.x,state.target.y-npc.position.y));
		Vec2 next(npc.position.x+direction.x*step,npc.position.y+direction.y*step);
		if(NpcStepClear(next,i)) {
			npc.position=next;
			state.phase+=step*9.0f;
		} else {
			state.moving=false; state.timer=0.7f+float(i%3)*0.3f;
		}
	}
}

void Update(float deltaTime)
{
	g_GameTime += deltaTime;
	if (g_DialogueTimer > 0.0f) g_DialogueTimer -= deltaTime;
	if (g_HitCooldown > 0.0f) g_HitCooldown -= deltaTime;
	if (g_AttackFlash > 0.0f) g_AttackFlash -= deltaTime;
	if (g_DodgeCooldown > 0.0f) g_DodgeCooldown -= deltaTime;

	Vec2 movement;
	g_PlayerMoving = false;
	if (g_Keys['w']) { movement.x -= 1.0f; movement.y -= 1.0f; }
	if (g_Keys['s']) { movement.x += 1.0f; movement.y += 1.0f; }
	if (g_Keys['a']) { movement.x -= 1.0f; movement.y += 1.0f; }
	if (g_Keys['d']) { movement.x += 1.0f; movement.y -= 1.0f; }

	if (movement.x != 0.0f || movement.y != 0.0f)
	{
		g_PlayerMoving = true;
		movement = Normalize(movement);
		g_Facing = movement;
		const float speed = 3.0f;
		TryMove(Vec2(movement.x * speed * deltaTime, movement.y * speed * deltaTime));
	}

	UpdateNpcWander(deltaTime);
	UpdateMonsters(deltaTime);
	UpdateAnimals(deltaTime);
	if (g_PlayerHealth <= 0)
	{
		g_PlayerHealth = 4;
		g_Player = Vec2(-5.5f, -4.5f);
		ShowDialogue(L"안내", L"마을의 등불이 당신을 어둠 속에서 끌어냈습니다.", 3.0f);
	}

	if (g_Quest == RETURN_TO_ELIA)
	{
		const Vec2 followDirection = Normalize(Vec2(g_Player.x - g_BoyPosition.x,
			g_Player.y - g_BoyPosition.y));
		if (Distance(g_Player, g_BoyPosition) > 0.9f)
		{
			g_BoyPosition.x += followDirection.x * 3.4f * deltaTime;
			g_BoyPosition.y += followDirection.y * 3.4f * deltaTime;
		}
	}
}

bool InitializeKoreanFont()
{
	HDC deviceContext = wglGetCurrentDC();
	if (deviceContext == NULL) return false;
	g_KoreanFont = CreateFontW(-20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
		HANGEUL_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
		FF_DONTCARE | DEFAULT_PITCH, L"맑은 고딕");
	if (g_KoreanFont == NULL) return false;

	g_FontReady = true;
	return g_FontReady;
}

GLuint GetFontGlyph(wchar_t character)
{
	std::map<wchar_t, GLuint>::const_iterator found = g_FontGlyphs.find(character);
	if (found != g_FontGlyphs.end()) return found->second;
	HDC deviceContext = wglGetCurrentDC();
	if (!g_FontReady || deviceContext == NULL) return 0;
	const GLuint list = glGenLists(1);
	if (list == 0) return 0;
	HGDIOBJ previousFont = SelectObject(deviceContext, g_KoreanFont);
	const BOOL created = wglUseFontBitmapsW(deviceContext, character, 1, list);
	SelectObject(deviceContext, previousFont);
	if (created == FALSE)
	{
		glDeleteLists(list, 1);
		return 0;
	}
	g_FontGlyphs[character] = list;
	return list;
}

void DestroyKoreanFont()
{
	for (std::map<wchar_t, GLuint>::const_iterator it = g_FontGlyphs.begin(); it != g_FontGlyphs.end(); ++it)
		glDeleteLists(it->second, 1);
	g_FontGlyphs.clear();
	if (g_KoreanFont != NULL) DeleteObject(g_KoreanFont);
	g_KoreanFont = NULL;
	g_FontReady = false;
}

void DrawGameText(float x, float y, const std::wstring& text, float r = 0.9f,
	float g = 0.9f, float b = 0.82f)
{
	glUseProgram(0);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glColor4f(r, g, b, 1.0f);
	glRasterPos2f(x * 2.0f / g_WindowWidth, y * 2.0f / g_WindowHeight);
	for (size_t i = 0; i < text.size(); ++i)
	{
		const wchar_t character = text[i];
		const GLuint glyph = GetFontGlyph(character);
		if (glyph != 0)
			glCallList(glyph);
		else if (character < 128)
			glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, static_cast<int>(character));
	}
}

void DrawGround()
{
	for (int y = -25; y <= 33; ++y)
	{
		for (int x = -28; x <= 32; ++x)
		{
			Vec2 world(static_cast<float>(x), static_cast<float>(y));
			Vec2 screen = Project(world);
			if (!IsVisible(screen, 70.0f)) continue;
			float r = 0.16f, g = 0.25f, b = 0.18f;
			bool isLake = false;
			bool isVillage = false;
			bool isRoad = false;

			const float lakeX = (world.x - 10.4f) / 4.0f;
			const float lakeY = (world.y - 8.8f) / 3.1f;
			if (lakeX * lakeX + lakeY * lakeY < 1.0f)
			{
				isLake = true;
				r = 0.07f; g = 0.20f; b = 0.25f;
				const float shimmer = 0.02f * std::sin(g_GameTime * 1.4f + x * 0.7f + y);
				g += shimmer; b += shimmer;
			}
			else if (RoadDistance(world) < 1.35f)
			{
				isVillage = true;
				isRoad = true;
				r = 0.43f; g = 0.39f; b = 0.30f;
			}
			else if (RoadDistance(world) < 1.85f)
			{
				isRoad = true;
				r = 0.25f; g = 0.24f; b = 0.18f;
			}
			else if ((x + y) % 2 == 0)
			{
				r += 0.015f; g += 0.018f; b += 0.01f;
			}

			g_Renderer->DrawDiamond(screen.x, screen.y, 0.0f, 56.0f, 30.0f, r, g, b, 1.0f);

			const int materialHash = std::abs(x * 19 + y * 31);
			if (isVillage)
			{

				// Four inset cobbles per tile leave continuous dark mortar seams.
				for(int u=0;u<2;++u) for(int v=0;v<2;++v) {
					Vec2 stone=Project(Vec2(world.x+(u-0.5f)*0.48f,world.y+(v-0.5f)*0.48f));
					float shade=0.035f*float((materialHash+u+v)%3);
					g_Renderer->DrawDiamond(stone.x,stone.y,0,24,12,
						0.52f+shade,0.47f+shade,0.37f+shade,1);
				}
			}
			else if (!isLake && materialHash % 7 == 0)
			{
				g_Renderer->DrawSolidRect(screen.x + (materialHash % 9) - 4.0f,
					screen.y + 3.0f, 0.0f, 2.0f, 9.0f, 0.24f, 0.34f, 0.20f, 0.58f);
			}
		}
	}
}

void DrawLakeEffects()
{
	const Vec2 lakeCenter = Project(Vec2(10.4f, 8.8f));
	if (!IsVisible(lakeCenter, 300.0f)) return;
	for (int i = 0; i < 5; ++i)
	{
		const float phase = std::fmod(g_GameTime * (14.0f + i * 2.0f) + i * 31.0f, 150.0f);
		const float alpha = (1.0f - phase / 150.0f) * 0.16f;
		g_Renderer->DrawCircle(lakeCenter.x + (i - 2) * 36.0f, lakeCenter.y + i * 5.0f,
			0.0f, 42.0f + phase, 9.0f + phase * 0.18f, 0.38f, 0.68f, 0.72f, alpha);
		g_Renderer->DrawCircle(lakeCenter.x + (i - 2) * 36.0f, lakeCenter.y + i * 5.0f,
			0.0f, 34.0f + phase, 5.0f + phase * 0.15f, 0.06f, 0.20f, 0.25f, alpha);
	}
}

void DrawHouse(const Vec2& position, int index)
{
	const Vec2 p = Project(position);
	if (!IsVisible(p)) return;
	const float warm = 0.06f * (index % 2);
	// Layered soft shadow cast by the building.
	g_Renderer->DrawDiamond(p.x + 20.0f, p.y - 2.0f, 0.0f, 145.0f, 34.0f, 0.01f, 0.012f, 0.015f, 0.18f);
	g_Renderer->DrawDiamond(p.x + 15.0f, p.y, 0.0f, 125.0f, 28.0f, 0.01f, 0.012f, 0.015f, 0.26f);
	// Plaster and stone wall material.
	g_Renderer->DrawSolidRect(p.x, p.y + 34.0f, 0.0f, 96.0f, 68.0f, 0.34f + warm, 0.29f, 0.22f, 1.0f);
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 5; ++column)
		{
			const float offset = (row % 2) * 8.0f;
			g_Renderer->DrawSolidRect(p.x - 37.0f + column * 18.0f + offset, p.y + 10.0f + row * 13.0f,
				0.0f, 14.0f, 3.0f, 0.22f, 0.20f, 0.17f, 0.42f);
		}
	}
	// Dark timber frame.
	g_Renderer->DrawSolidRect(p.x - 43.0f, p.y + 35.0f, 0.0f, 7.0f, 66.0f, 0.14f, 0.075f, 0.04f, 1.0f);
	g_Renderer->DrawSolidRect(p.x + 43.0f, p.y + 35.0f, 0.0f, 7.0f, 66.0f, 0.14f, 0.075f, 0.04f, 1.0f);
	g_Renderer->DrawSolidRect(p.x, p.y + 35.0f, 0.0f, 7.0f, 66.0f, 0.14f, 0.075f, 0.04f, 1.0f);
	g_Renderer->DrawSolidRect(p.x, p.y + 54.0f, 0.0f, 92.0f, 6.0f, 0.14f, 0.075f, 0.04f, 1.0f);

	// A pitched roof: broad trapezoid slope, shaded hip and substantial eaves.
	const float roof[] = {p.x-61,p.y+64,p.x+49,p.y+64,p.x+29,p.y+111,p.x-30,p.y+111};
	g_Renderer->DrawQuad(roof,0.27f,0.12f,0.075f,1);
	const float hip[] = {p.x+49,p.y+64,p.x+68,p.y+76,p.x+42,p.y+116,p.x+29,p.y+111};
	g_Renderer->DrawQuad(hip,0.19f,0.08f,0.055f,1);
	for(int row=0;row<6;++row) {
		float t0=row/6.0f, t1=(row+0.88f)/6.0f;
		float l0=-61+31*t0, r0=49-20*t0;
		float l1=-61+31*t1, r1=49-20*t1;
		int columns=10-row/2;
		for(int col=0;col<columns;++col) {
			float u0=float(col)/columns+0.012f,u1=float(col+1)/columns-0.012f;
			float tile[]={p.x+l0+(r0-l0)*u0,p.y+64+47*t0,
				p.x+l0+(r0-l0)*u1,p.y+64+47*t0,
				p.x+l1+(r1-l1)*u1,p.y+64+47*t1,
				p.x+l1+(r1-l1)*u0,p.y+64+47*t1};
			float variation=0.025f*((row*7+col*3+index)%4);
			g_Renderer->DrawQuad(tile,0.43f+variation,0.21f+variation,0.12f+variation,1);
		}
	}
	g_Renderer->DrawSolidRect(p.x-6,p.y+63,0,114,5,0.12f,0.065f,0.035f,1);
	g_Renderer->DrawSolidRect(p.x,p.y+113,0,64,5,0.52f,0.29f,0.16f,1);

	g_Renderer->DrawSolidRect(p.x, p.y + 19.0f, 0.0f, 20.0f, 38.0f, 0.15f, 0.09f, 0.06f, 1.0f);
	for (int side = -1; side <= 1; side += 2)
	{
		const float windowX = p.x + side * 28.0f;
		g_Renderer->DrawSolidRect(windowX, p.y + 43.0f, 0.0f, 21.0f, 23.0f, 0.10f, 0.07f, 0.05f, 1.0f);
		g_Renderer->DrawSolidRect(windowX, p.y + 43.0f, 0.0f, 14.0f, 16.0f, 0.96f, 0.54f, 0.18f, 0.90f);
		g_Renderer->DrawSolidRect(windowX, p.y + 43.0f, 0.0f, 2.0f, 16.0f, 0.16f, 0.09f, 0.05f, 1.0f);
	}
}

void DrawTree(const Vec2& position, int index)
{
	const Vec2 p=Project(position);
	if(!IsVisible(p,180)) return;
	float scale=0.8f+(index%5)*0.12f;
	float sway=std::sin(g_GameTime*1.2f+index)*2.0f;
	g_Renderer->DrawCircle(p.x+18,p.y-3,0,78*scale,22,0.015f,0.02f,0.028f,0.26f);
	g_Renderer->DrawCircle(p.x+6,p.y,0,38*scale,11,0.01f,0.015f,0.02f,0.4f);
	g_Renderer->DrawSolidRect(p.x,p.y+34*scale,0,14*scale,68*scale,0.24f,0.16f,0.095f,1);
	for(int bark=0;bark<3;++bark)
		g_Renderer->DrawSolidRect(p.x-5+bark*4,p.y+30*scale,0,2,50*scale,0.12f,0.08f,0.05f,0.7f);
	for(int branch=-1;branch<=1;branch+=2) {
		float q[]={p.x-4,p.y+30*scale,p.x+5,p.y+34*scale,
			p.x+branch*29*scale,p.y+65*scale,p.x+branch*24*scale,p.y+66*scale};
		g_Renderer->DrawQuad(q,0.22f,0.14f,0.08f,1);
	}
	// Distinct crowns: layered conifers and irregular broadleaf clusters.
	if(index%3==0) {
		for(int layer=0;layer<4;++layer) {
			float w=(75-layer*14)*scale, y=p.y+(45+layer*19)*scale;
			float q[]={p.x-w/2+sway,y,p.x+w/2+sway,y,
				p.x+5+sway,y+39*scale,p.x-5+sway,y+39*scale};
			g_Renderer->DrawQuad(q,0.085f+layer*0.015f,0.20f+layer*0.025f,0.15f,1);
		}
	} else {
		for(int cluster=0;cluster<8;++cluster) {
			float angle=cluster*2.399f+index;
			float cx=p.x+std::cos(angle)*25*scale+sway;
			float cy=p.y+(75+std::sin(angle)*20)*scale;
			g_Renderer->DrawCircle(cx,cy,0,53*scale,44*scale,0.095f,0.22f,0.14f,1);
			for(int leaf=0;leaf<5;++leaf) {
				float la=leaf*1.7f+cluster;
				g_Renderer->DrawCircle(cx+std::cos(la)*15*scale,cy+std::sin(la)*12*scale,
					0,15*scale,9*scale,0.16f,0.31f,0.19f,0.65f);
			}
		}
	}
}

void DrawCharacter(const Vec2& position, float r, float g, float b, bool player, float npcMotion = 0.0f, float npcPhase = 0.0f)
{
	const Vec2 p = Project(position);
	if (!IsVisible(p)) return;
	const float moveAmount = player ? (g_PlayerMoving ? 1.0f : 0.0f) : npcMotion;
	const float phase = player ? g_GameTime * 10.0f : npcPhase;
	const float walk = std::sin(phase) * 5.0f * moveAmount;
	const float bob = std::fabs(std::sin(phase)) * 2.0f * moveAmount;

	// Soft contact shadow with a wider translucent penumbra.
	g_Renderer->DrawCircle(p.x + 5.0f, p.y, 0.0f, 43.0f, 14.0f, 0.01f, 0.012f, 0.018f, 0.16f);
	g_Renderer->DrawCircle(p.x + 2.0f, p.y + 1.0f, 0.0f, 31.0f, 9.0f, 0.01f, 0.012f, 0.018f, 0.34f);
	// Animated legs and boots.
	g_Renderer->DrawSolidRect(p.x - 7.0f + walk * 0.35f, p.y + 12.0f, 0.0f, 7.0f, 22.0f, 0.13f, 0.12f, 0.13f, 1.0f);
	g_Renderer->DrawSolidRect(p.x + 7.0f - walk * 0.35f, p.y + 12.0f, 0.0f, 7.0f, 22.0f, 0.13f, 0.12f, 0.13f, 1.0f);
	g_Renderer->DrawSolidRect(p.x - 8.0f + walk * 0.55f, p.y + 3.0f, 0.0f, 12.0f, 7.0f, 0.07f, 0.055f, 0.045f, 1.0f);
	g_Renderer->DrawSolidRect(p.x + 8.0f - walk * 0.55f, p.y + 3.0f, 0.0f, 12.0f, 7.0f, 0.07f, 0.055f, 0.045f, 1.0f);
	// Tunic, belt, arms and cloak create a readable medieval silhouette.
	g_Renderer->DrawDiamond(p.x, p.y + 29.0f + bob, 0.0f, player ? 34.0f : 30.0f, 46.0f, r * 0.65f, g * 0.65f, b * 0.65f, 1.0f);
	g_Renderer->DrawSolidRect(p.x, p.y + 32.0f + bob, 0.0f, player ? 25.0f : 22.0f, 38.0f, r, g, b, 1.0f);
	g_Renderer->DrawSolidRect(p.x, p.y + 25.0f + bob, 0.0f, 28.0f, 5.0f, 0.20f, 0.11f, 0.055f, 1.0f);
	g_Renderer->DrawSolidRect(p.x - 17.0f - walk * 0.25f, p.y + 32.0f + bob, 0.0f, 7.0f, 29.0f, r * 0.82f, g * 0.82f, b * 0.82f, 1.0f);
	g_Renderer->DrawSolidRect(p.x + 17.0f + walk * 0.25f, p.y + 32.0f + bob, 0.0f, 7.0f, 29.0f, r * 0.82f, g * 0.82f, b * 0.82f, 1.0f);
	// Neck, round head, hair and face highlight.
	g_Renderer->DrawSolidRect(p.x, p.y + 51.0f + bob, 0.0f, 8.0f, 9.0f, 0.55f, 0.38f, 0.28f, 1.0f);
	g_Renderer->DrawCircle(p.x, p.y + 61.0f + bob, 0.0f, 23.0f, 25.0f, 0.70f, 0.52f, 0.38f, 1.0f);
	g_Renderer->DrawCircle(p.x - 2.0f, p.y + 68.0f + bob, 0.0f, 23.0f, 13.0f,
		player ? 0.12f : 0.20f, player ? 0.12f : 0.14f, player ? 0.16f : 0.10f, 1.0f);
	g_Renderer->DrawSolidRect(p.x + 6.0f, p.y + 62.0f + bob, 0.0f, 3.0f, 3.0f, 0.86f, 0.76f, 0.56f, 0.85f);
	if (player)
	{
		g_Renderer->DrawSolidRect(p.x - 20.0f, p.y + 29.0f + bob, 0.0f, 5.0f, 38.0f, 0.72f, 0.72f, 0.66f, 1.0f);
		if (g_AttackFlash > 0.0f)
		{
			const Vec2 projectedFacing((g_Facing.x - g_Facing.y) * 24.0f,
				-(g_Facing.x + g_Facing.y) * 12.0f);
			g_Renderer->DrawDiamond(p.x + projectedFacing.x, p.y + 25.0f + projectedFacing.y,
				0.0f, 48.0f, 18.0f, 0.90f, 0.78f, 0.38f, 0.75f);
		}
	}
}

void DrawMonster(const Monster& monster)
{
	const Vec2 p = Project(monster.position);
	const float pulse = 3.0f * std::sin(g_GameTime * 7.0f);
	g_Renderer->DrawDiamond(p.x, p.y + 13.0f, 0.0f, 42.0f + pulse, 31.0f, 0.25f, 0.06f, 0.09f, 0.95f);
	g_Renderer->DrawSolidRect(p.x - 8.0f, p.y + 17.0f, 0.0f, 5.0f, 4.0f, 0.95f, 0.72f, 0.30f, 1.0f);
	g_Renderer->DrawSolidRect(p.x + 8.0f, p.y + 17.0f, 0.0f, 5.0f, 4.0f, 0.95f, 0.72f, 0.30f, 1.0f);
}

void DrawAnimal(const Animal& animal)
{
	const Vec2 p = Project(animal.position);
	if (!IsVisible(p)) return;
	const float step = std::sin(g_GameTime * 8.0f + animal.animationOffset) * 4.0f;
	float r = 0.42f, g = 0.30f, b = 0.20f;
	float bodyWidth = 42.0f, bodyHeight = 24.0f;
	if (animal.kind == Animal::DEER) { r = 0.48f; g = 0.34f; b = 0.20f; bodyWidth = 46.0f; }
	if (animal.kind == Animal::WOLF) { r = 0.30f; g = 0.33f; b = 0.35f; bodyWidth = 44.0f; }

	g_Renderer->DrawCircle(p.x + 6.0f, p.y, 0.0f, 54.0f, 13.0f, 0.01f, 0.015f, 0.016f, 0.30f);
	g_Renderer->DrawSolidRect(p.x - 12.0f + step * 0.2f, p.y + 9.0f, 0.0f, 5.0f, 20.0f, r * 0.55f, g * 0.55f, b * 0.55f, 1.0f);
	g_Renderer->DrawSolidRect(p.x + 13.0f - step * 0.2f, p.y + 9.0f, 0.0f, 5.0f, 20.0f, r * 0.55f, g * 0.55f, b * 0.55f, 1.0f);
	g_Renderer->DrawCircle(p.x, p.y + 23.0f, 0.0f, bodyWidth, bodyHeight, r, g, b, 1.0f);
	g_Renderer->DrawCircle(p.x + 23.0f, p.y + 31.0f, 0.0f, 22.0f, 20.0f, r * 0.92f, g * 0.92f, b * 0.92f, 1.0f);
	g_Renderer->DrawCircle(p.x + 31.0f, p.y + 30.0f, 0.0f, 4.0f, 4.0f, 0.07f, 0.05f, 0.04f, 1.0f);

	if (animal.kind == Animal::DEER)
	{
		g_Renderer->DrawSolidRect(p.x + 19.0f, p.y + 45.0f, 0.0f, 3.0f, 19.0f, 0.25f, 0.15f, 0.08f, 1.0f);
		g_Renderer->DrawSolidRect(p.x + 28.0f, p.y + 45.0f, 0.0f, 3.0f, 19.0f, 0.25f, 0.15f, 0.08f, 1.0f);
	}
	else if (animal.kind == Animal::BOAR)
	{
		g_Renderer->DrawSolidRect(p.x + 35.0f, p.y + 28.0f, 0.0f, 9.0f, 3.0f, 0.82f, 0.76f, 0.60f, 1.0f);
	}
	else
	{
		g_Renderer->DrawDiamond(p.x - 25.0f, p.y + 26.0f, 0.0f, 24.0f, 11.0f, r, g, b, 1.0f);
		g_Renderer->DrawCircle(p.x + 24.0f, p.y + 34.0f, 0.0f, 4.0f, 4.0f, 0.78f, 0.20f, 0.12f, 1.0f);
	}
}

void DrawLantern(const Vec2& position)
{
	const Vec2 p = Project(position);
	if (!IsVisible(p)) return;
	const float flicker = 0.92f + std::sin(g_GameTime * 11.0f + position.x) * 0.08f;
	// Additive-looking glow is approximated with layered transparent circles.
	g_Renderer->DrawCircle(p.x, p.y + 49.0f, 0.0f, 100.0f, 82.0f, 0.95f, 0.40f, 0.08f, 0.045f * flicker);
	g_Renderer->DrawCircle(p.x, p.y + 49.0f, 0.0f, 62.0f, 52.0f, 1.0f, 0.52f, 0.12f, 0.09f * flicker);
	g_Renderer->DrawSolidRect(p.x, p.y + 24.0f, 0.0f, 5.0f, 48.0f, 0.14f, 0.10f, 0.07f, 1.0f);
	g_Renderer->DrawDiamond(p.x, p.y + 50.0f, 0.0f, 21.0f, 28.0f, 0.22f, 0.14f, 0.07f, 1.0f);
	g_Renderer->DrawCircle(p.x, p.y + 50.0f, 0.0f, 10.0f, 15.0f, 1.0f, 0.64f, 0.18f, 0.95f);
}

void DrawMarker(const Vec2& position, float r, float g, float b)
{
	const Vec2 p = Project(position);
	const float hover = 5.0f * std::sin(g_GameTime * 3.0f);
	g_Renderer->DrawDiamond(p.x, p.y + 78.0f + hover, 0.0f, 18.0f, 24.0f, r, g, b, 0.95f);
}

void DrawWorldObjects()
{
	std::vector<WorldObject> objects;
	for (size_t i = 0; i < g_Houses.size(); ++i)
		objects.push_back({ WorldObject::HOUSE, g_Houses[i], static_cast<int>(i), g_Houses[i].x + g_Houses[i].y });
	for (size_t i = 0; i < g_Trees.size(); ++i)
		objects.push_back({ WorldObject::TREE, g_Trees[i], static_cast<int>(i), g_Trees[i].x + g_Trees[i].y });
	for (size_t i = 0; i < g_Lanterns.size(); ++i)
		objects.push_back({ WorldObject::LANTERN, g_Lanterns[i], static_cast<int>(i), g_Lanterns[i].x + g_Lanterns[i].y });
	for (size_t i = 0; i < g_Npcs.size(); ++i)
		objects.push_back({ WorldObject::NPC_OBJECT, g_Npcs[i].position, static_cast<int>(i), g_Npcs[i].position.x + g_Npcs[i].position.y });
	objects.push_back({ WorldObject::NPC_OBJECT, kEliaPosition, -1, kEliaPosition.x + kEliaPosition.y });
	objects.push_back({ WorldObject::PLAYER_OBJECT, g_Player, 0, g_Player.x + g_Player.y });

	if (g_Quest == FIND_BOOT)
		objects.push_back({ WorldObject::CLUE_OBJECT, kBootPosition, 0, kBootPosition.x + kBootPosition.y });
	if (g_Quest == FIND_REEDS)
		objects.push_back({ WorldObject::CLUE_OBJECT, kReedsPosition, 1, kReedsPosition.x + kReedsPosition.y });
	if (g_Quest == FIND_BOY || g_Quest == RETURN_TO_ELIA || g_Quest == QUEST_COMPLETE)
		objects.push_back({ WorldObject::BOY_OBJECT, g_BoyPosition, 0, g_BoyPosition.x + g_BoyPosition.y });
	for (size_t i = 0; i < g_Monsters.size(); ++i)
	{
		if (g_Monsters[i].alive)
			objects.push_back({ WorldObject::MONSTER_OBJECT, g_Monsters[i].position, static_cast<int>(i), g_Monsters[i].position.x + g_Monsters[i].position.y });
	}
	for (size_t i = 0; i < g_Animals.size(); ++i)
	{
		if (g_Animals[i].alive && IsVisible(Project(g_Animals[i].position), 180.0f))
			objects.push_back({ WorldObject::ANIMAL_OBJECT, g_Animals[i].position, static_cast<int>(i), g_Animals[i].position.x + g_Animals[i].position.y });
	}

	std::stable_sort(objects.begin(), objects.end(), [](const WorldObject& a, const WorldObject& b)
	{
		// OpenGL screen Y grows upward: high feet first, low feet last.
		return ProjectRaw(a.position).y > ProjectRaw(b.position).y;
	});

	for (size_t i = 0; i < objects.size(); ++i)
	{
		const WorldObject& object = objects[i];
		switch (object.type)
		{
		case WorldObject::HOUSE:
			DrawHouse(object.position, object.index);
			break;
		case WorldObject::TREE:
			DrawTree(object.position, object.index);
			break;
		case WorldObject::LANTERN:
			DrawLantern(object.position);
			break;
		case WorldObject::NPC_OBJECT:
			if (object.index < 0)
			{
				DrawCharacter(object.position, 0.55f, 0.20f, 0.22f, false);
				if (g_Quest == MEET_ELIA || g_Quest == RETURN_TO_ELIA)
					DrawMarker(object.position, 0.95f, 0.72f, 0.18f);
			}
			else
			{
				const Npc& npc = g_Npcs[object.index];
				const NpcWander& motion=g_NpcWander[object.index];
				DrawCharacter(npc.position, npc.r, npc.g, npc.b, false,
					motion.moving ? 1.0f : 0.0f, motion.phase);
			}
			break;
		case WorldObject::PLAYER_OBJECT:
			DrawCharacter(g_Player, g_HitCooldown > 0.0f ? 0.75f : 0.18f, 0.32f, 0.52f, true);
			break;
		case WorldObject::MONSTER_OBJECT:
			DrawMonster(g_Monsters[object.index]);
			break;
		case WorldObject::ANIMAL_OBJECT:
			DrawAnimal(g_Animals[object.index]);
			break;
		case WorldObject::BOY_OBJECT:
			DrawCharacter(g_BoyPosition, 0.25f, 0.42f, 0.46f, false);
			if (g_Quest == FIND_BOY)
				DrawMarker(g_BoyPosition, 0.95f, 0.72f, 0.18f);
			break;
		case WorldObject::CLUE_OBJECT:
		{
			const Vec2 p = Project(object.position);
			if (object.index == 0)
				g_Renderer->DrawSolidRect(p.x, p.y + 4.0f, 0.0f, 16.0f, 9.0f, 0.38f, 0.22f, 0.12f, 1.0f);
			else
				g_Renderer->DrawDiamond(p.x, p.y + 8.0f, 0.0f, 28.0f, 28.0f, 0.08f, 0.05f, 0.11f, 1.0f);
			DrawMarker(object.position, 0.60f, 0.80f, 0.84f);
			break;
		}
		}
	}
}

std::wstring ObjectiveText()
{
	switch (g_Quest)
	{
	case MEET_ELIA: return L"마을 광장에서 엘리아와 대화하십시오 [E]";
	case FIND_BOOT: return L"동쪽 숲길을 따라가 아렌의 흔적을 찾으십시오";
	case FIND_REEDS: return L"젖은 발자국을 따라 호수로 이동하십시오";
	case DEFEAT_CREATURES: return L"안개 괴물을 물리치십시오 [F 공격 / SPACE 회피]";
	case FIND_BOY: return L"호숫가에서 아렌을 찾으십시오";
	case RETURN_TO_ELIA: return L"아렌을 엘리아에게 데려가십시오";
	case QUEST_COMPLETE: return L"퀘스트 완료 - 호수 아래의 종";
	default: return L"";
	}
}

void DrawUI()
{
	const float left = -g_WindowWidth * 0.5f;
	const float top = g_WindowHeight * 0.5f;

	g_Renderer->DrawSolidRect(left + 250.0f, top - 48.0f, 0.0f, 470.0f, 68.0f, 0.025f, 0.035f, 0.045f, 0.83f);
	DrawGameText(left + 28.0f, top - 35.0f, L"호수 아래의 종", 0.84f, 0.70f, 0.42f);
	DrawGameText(left + 28.0f, top - 62.0f, ObjectiveText(), 0.93f, 0.91f, 0.80f);

	for (int i = 0; i < 4; ++i)
	{
		const bool filled = i < g_PlayerHealth;
		g_Renderer->DrawDiamond(left + 34.0f + i * 28.0f, top - 102.0f, 0.0f, 20.0f, 18.0f,
			filled ? 0.63f : 0.16f, filled ? 0.12f : 0.14f, filled ? 0.14f : 0.15f, 0.95f);
	}

	int seconds = static_cast<int>(g_GameTime);
	std::wstring timeText = L"튜토리얼 시간  " + std::to_wstring(seconds / 60) + L":" + (seconds % 60 < 10 ? L"0" : L"") + std::to_wstring(seconds % 60) + L" / 5:00";
	DrawGameText(g_WindowWidth * 0.5f - 245.0f, top - 36.0f, timeText, 0.72f, 0.77f, 0.76f);

	if (g_DialogueTimer > 0.0f)
	{
		g_Renderer->DrawSolidRect(0.0f, -g_WindowHeight * 0.5f + 78.0f, 0.0f,
			g_WindowWidth * 0.78f, 108.0f, 0.02f, 0.025f, 0.035f, 0.90f);
		DrawGameText(-g_WindowWidth * 0.34f, -g_WindowHeight * 0.5f + 104.0f,
			g_Speaker, 0.86f, 0.66f, 0.35f);
		DrawGameText(-g_WindowWidth * 0.34f, -g_WindowHeight * 0.5f + 70.0f,
			g_Dialogue, 0.93f, 0.92f, 0.85f);
	}

	std::wstring prompt;
	if (Distance(g_Player, kEliaPosition) < 1.35f ||
		(g_Quest == FIND_BOOT && Distance(g_Player, kBootPosition) < 1.1f) ||
		(g_Quest == FIND_REEDS && Distance(g_Player, kReedsPosition) < 1.1f) ||
		(g_Quest == FIND_BOY && Distance(g_Player, g_BoyPosition) < 1.35f))
		prompt = L"[E] 상호작용";
	else
	{
		for (size_t i = 0; i < g_Npcs.size(); ++i)
			if (Distance(g_Player, g_Npcs[i].position) < 1.1f) prompt = L"[E] 대화";
	}
	if (!prompt.empty())
	{
		g_Renderer->DrawSolidRect(0.0f, -80.0f, 0.0f, 150.0f, 42.0f, 0.02f, 0.03f, 0.04f, 0.82f);
		DrawGameText(-52.0f, -86.0f, prompt, 0.96f, 0.78f, 0.32f);
	}

	DrawGameText(left + 25.0f, -top + 24.0f,
		L"WASD 이동   E 상호작용   F 공격   SPACE 회피   R 재시작   ESC 종료",
		0.63f, 0.68f, 0.67f);
}

void RenderScene()
{
	g_Renderer->BeginScene(0.025f, 0.055f, 0.065f, 1.0f);
	DrawGround();
	DrawLakeEffects();
	DrawWorldObjects();

	// Atmospheric color veil. Vignette, grading, fog and grain are applied
	// in the post-processing pass below.
	g_Renderer->DrawSolidRect(0.0f, 0.0f, 0.0f, static_cast<float>(g_WindowWidth),
		static_cast<float>(g_WindowHeight), 0.06f, 0.10f, 0.13f, 0.07f);


	for(int i=0;i<16;++i) g_Renderer->SetLight(i,0,0,0);
	for(size_t i=0;i<g_Lanterns.size() && i<16;++i) {
		Vec2 p=Project(g_Lanterns[i]);
		g_Renderer->SetLight(static_cast<int>(i),p.x,p.y+28,190);
	}
	g_Renderer->EndScene(g_GameTime);
	DrawUI();
	glutSwapBuffers();
}

void Resize(int width, int height)
{
	g_WindowWidth = width > 1 ? width : 1;
	g_WindowHeight = height > 1 ? height : 1;
	if (g_Renderer)
		g_Renderer->SetViewport(g_WindowWidth, g_WindowHeight);
}

void KeyDown(unsigned char key, int, int)
{
	key = static_cast<unsigned char>(std::tolower(key));
	g_Keys[key] = true;
	if (key == 27)
		glutLeaveMainLoop();
	else if (key == 'e')
		Interact();
	else if (key == 'f')
		Attack();
	else if (key == ' ')
		Dodge();
	else if (key == 'r')
		ResetGame();
}

void KeyUp(unsigned char key, int, int)
{
	key = static_cast<unsigned char>(std::tolower(key));
	g_Keys[key] = false;
}

void Timer(int)
{
	const int now = glutGet(GLUT_ELAPSED_TIME);
	float deltaTime = (now - g_LastUpdateMs) / 1000.0f;
	if (deltaTime > 0.05f) deltaTime = 0.05f;
	g_LastUpdateMs = now;
	Update(deltaTime);
	glutPostRedisplay();
	glutTimerFunc(16, Timer, 0);
}

int main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(80, 60);
	glutInitWindowSize(g_WindowWidth, g_WindowHeight);
	glutCreateWindow("Greyhaven Tutorial Prototype");
	SetWindowTextW(GetActiveWindow(), L"회암 마을 - 쿼터뷰 RPG 튜토리얼");
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	glewExperimental = GL_TRUE;
	const GLenum glewResult = glewInit();
	if (glewResult != GLEW_OK)
	{
		std::cerr << "GLEW initialization failed." << std::endl;
		return 1;
	}

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	g_Renderer = new Renderer(g_WindowWidth, g_WindowHeight);
	if (!g_Renderer->IsInitialized())
	{
		std::cerr << "Renderer could not be initialized." << std::endl;
		delete g_Renderer;
		return 1;
	}
	if (!InitializeKoreanFont())
		std::cerr << "Korean font initialization failed." << std::endl;

	InitializeWorld();
	g_LastUpdateMs = glutGet(GLUT_ELAPSED_TIME);

	glutDisplayFunc(RenderScene);
	glutReshapeFunc(Resize);
	glutKeyboardFunc(KeyDown);
	glutKeyboardUpFunc(KeyUp);
	glutIgnoreKeyRepeat(1);
	glutTimerFunc(16, Timer, 0);
	glutMainLoop();

	DestroyKoreanFont();
	delete g_Renderer;
	g_Renderer = NULL;
	return 0;
}
