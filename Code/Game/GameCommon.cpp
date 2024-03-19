#pragma once
#include "GameCommon.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"

//--------------------------------------------------------------------------------------------------------------------------------------------------------
RandomNumberGenerator g_rng;
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void DebugDrawLine(Vec2 const& start, Vec2 const& end, float thickness, Rgba8 const& color)
{
	float radius = thickness * 0.5f;
	Vec2 displacement = end - start;
	Vec2 forwardDir = displacement.GetNormalized();
	Vec2 forwardStep = forwardDir * radius;
	Vec2 leftStep = forwardStep.GetRotated90Degrees();

	Vec2 endLeft = end + forwardStep + leftStep;
	Vec2 endRight = end + forwardStep - leftStep;
	Vec2 startLeft = start - forwardStep + leftStep;
	Vec2 startRight = start - forwardStep - leftStep;

	Vertex_PCU verts[] =
	{
		Vertex_PCU(startLeft,color),
		Vertex_PCU(startRight,color),
		Vertex_PCU(endRight,color),

		Vertex_PCU(endRight,color),
		Vertex_PCU(endLeft,color),
		Vertex_PCU(startLeft,color),
	};
	constexpr int NUM_VERTS = sizeof(verts) / sizeof(Vertex_PCU);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->DrawVertexArray(NUM_VERTS, verts);
}

//--------------------------------------------------------------------------------------------------------------------------------------------------------
void DebugDrawRing(Vec2 const& center, float radius, float thickness, Rgba8 const& color)
{
	float halfThickness = 0.5f * thickness;
	float innerRadius = radius - halfThickness;
	float outerRadius = radius + halfThickness;
	constexpr int NUM_SIDES = 24;
	constexpr int NUM_TRIS = 2 * NUM_SIDES;
	constexpr int NUM_VERTS = 3 * NUM_TRIS;
	constexpr float DEGREES_PER_SIDE = 360.f / (float)NUM_SIDES;
	Vertex_PCU verts[NUM_VERTS];

	for (int sideNum = 0; sideNum < NUM_SIDES; sideNum++)
	{
		float startDegrees = DEGREES_PER_SIDE * (float)sideNum;
		float endDegrees = DEGREES_PER_SIDE * (float)(sideNum + 1);
		float cosStart = CosDegrees(startDegrees);
		float sinStart = SinDegrees(startDegrees);
		float cosEnd = CosDegrees(endDegrees);
		float sinEnd = SinDegrees(endDegrees);
		Vec3 innerStartPos = Vec3(center.x + innerRadius * cosStart, center.y + innerRadius * sinStart, 0.f);
		Vec3 outerStartPos = Vec3(center.x + outerRadius * cosStart, center.y + outerRadius * sinStart, 0.f);
		Vec3 outerEndPos = Vec3(center.x + outerRadius * cosEnd, center.y + outerRadius * sinEnd, 0.f);
		Vec3 innerEndPos = Vec3(center.x + innerRadius * cosEnd, center.y + innerRadius * sinEnd, 0.f);

		int vertIndexA = (6 * sideNum) + 0;
		int vertIndexB = (6 * sideNum) + 1;
		int vertIndexC = (6 * sideNum) + 2;
		int vertIndexD = (6 * sideNum) + 3;
		int vertIndexE = (6 * sideNum) + 4;
		int vertIndexF = (6 * sideNum) + 5;


		verts[vertIndexA].m_position = innerEndPos;
		verts[vertIndexB].m_position = innerStartPos;
		verts[vertIndexC].m_position = outerStartPos;
		verts[vertIndexA].m_color = color;
		verts[vertIndexB].m_color = color;
		verts[vertIndexC].m_color = color;

		verts[vertIndexD].m_position = innerEndPos;
		verts[vertIndexE].m_position = outerStartPos;
		verts[vertIndexF].m_position = outerEndPos;
		verts[vertIndexD].m_color = color;
		verts[vertIndexE].m_color = color;
		verts[vertIndexF].m_color = color;
	};
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->DrawVertexArray(NUM_VERTS, verts);
}

