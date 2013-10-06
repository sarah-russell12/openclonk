/*
* OpenClonk, http://www.openclonk.org
*
* Copyright (c) 2013 David Dormagen
*
* Portions might be copyrighted by other authors who have contributed
* to OpenClonk.
*
* Permission to use, copy, modify, and/or distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
* See isc_license.txt for full license and disclaimer.
*
* "Clonk" is a registered trademark of Matthes Bender.
* See clonk_trademark_license.txt for full license.
*/

#include <C4Particles.h>
#include <StdScheduler.h>

#ifndef INC_C4DynamicParticles
#define INC_C4DynamicParticles

enum C4ParticleValueProviderID
{
	C4PV_Const,
	C4PV_Linear,
	C4PV_Random,
	C4PV_KeyFrames,
	C4PV_Direction,
	C4PV_Step,
	C4PV_Speed,
};

enum C4ParticleCollisionFuncID
{
	C4PC_Die,
	C4PC_Bounce,
};

class C4DynamicParticleList;
class C4DynamicParticleChunk;
class C4DynamicParticle;
class C4DynamicParticleProperties;
class C4DynamicParticleValueProvider;

typedef float (C4DynamicParticleValueProvider::*C4DynamicParticleValueProviderFunction) (C4DynamicParticle*);
typedef bool (C4DynamicParticleProperties::*C4DynamicParticleCollisionCallback) (C4DynamicParticle*);

class C4DynamicParticleValueProvider
{
protected:
	float startValue, endValue;

	// used by Random
	float currentValue;

	union
	{
		int rerollInterval; // for Random
		int keyFrameCount; // for KeyFrames
		float delay; // for Step
		float speedFactor; // for Speed
	};
	int smoothing;
	float *keyFrames;

	C4DynamicParticleValueProviderFunction valueFunction;
	bool isConstant;
public:
	bool IsConstant() { return isConstant; }
	C4DynamicParticleValueProvider() : startValue(0.f), endValue(0.f), currentValue(0.f), rerollInterval(0), smoothing(0), keyFrames(0), valueFunction(0), isConstant(true) { }
	~C4DynamicParticleValueProvider()
	{
		if (keyFrames != 0) free(keyFrames);
	}
	C4DynamicParticleValueProvider(const C4DynamicParticleValueProvider &other) { *this = other; }
	C4DynamicParticleValueProvider & operator= (const C4DynamicParticleValueProvider &other);
	void RollRandom();

	// divides by denominator
	void Floatify(float denominator);

	void Set(float _startValue, float _endValue = 1.f, C4ParticleValueProviderID what = C4PV_Const);
	void Set(const C4Value &value);
	void Set(C4ValueArray *fromArray);
	float GetValue(C4DynamicParticle *forParticle);

	float Linear(C4DynamicParticle *forParticle);
	float Const(C4DynamicParticle *forParticle);
	float Random(C4DynamicParticle *forParticle);
	float KeyFrames(C4DynamicParticle *forParticle);
	float Direction(C4DynamicParticle *forParticle);
	float Step(C4DynamicParticle *forParticle);
	float Speed(C4DynamicParticle *forParticle);
};

class C4DynamicParticleProperties
{
public:
	bool hasConstantColor;
	bool hasCollisionVertex;

	C4DynamicParticleValueProvider size, stretch;
	C4DynamicParticleValueProvider forceX, forceY;
	C4DynamicParticleValueProvider speedDampingX, speedDampingY;
	C4DynamicParticleValueProvider colorR, colorG, colorB, colorAlpha;
	C4DynamicParticleValueProvider rotation;
	C4DynamicParticleValueProvider phase;
	C4DynamicParticleValueProvider collisionVertex;

	float bouncyness;
	C4DynamicParticleCollisionCallback collisionCallback;
	void SetCollisionFunc(const C4Value &source);

	int blitMode;

	C4DynamicParticleProperties();

	
	void Set(C4PropList *dataSource);
	// divides ints in certain properties by 1000f and in the color properties by 255f
	void Floatify();

	
	bool CollisionDie(C4DynamicParticle *forParticle) { return false; }
	bool CollisionBounce(C4DynamicParticle *forParticle);
};

class C4DynamicParticle
{
public:

	struct DrawingData
	{
		static const int vertexCountPerParticle;

		struct Vertex
		{
			float x;
			float y;

			float u;
			float v;
			
			float r;
			float g;
			float b;
			float alpha;
		};
		Vertex *vertices;
		
		int phase;

		float currentStretch;
		float originalSize;
		float sizeX, sizeY;
		float aspect;

		void SetPointer(Vertex *startingVertex, bool initial = false)
		{
			vertices = startingVertex;

			if (initial)
			{
				vertices[0].u = 0.f; vertices[0].v = 1.f;
				vertices[1].u = 0.f; vertices[1].v = 0.f;
				vertices[2].u = 1.f; vertices[2].v = 1.f;
				vertices[3].u = 1.f; vertices[3].v = 0.f;

				SetColor(1.f, 1.f, 1.f, 1.f);

				phase = -1;
			}
		}

		void SetColor(float r, float g, float b, float a = 1.0f)
		{
			for (int vertex = 0; vertex < 4; ++vertex)
			{
				vertices[vertex].r = r;
				vertices[vertex].g = g;
				vertices[vertex].b = b;
				vertices[vertex].alpha = a;
			}
		}

		void SetPosition(float x, float y, float size, float rotation = 0.f, float stretch = 1.f);
		void SetPhase(int phase, C4ParticleDef *sourceDef);

		DrawingData() : currentStretch(1.f), originalSize(0.f), aspect(1.f)
		{
		}

	} drawingData;
protected:
	float currentSpeedX, currentSpeedY;
	float positionX, positionY;
	float lifetime, startingLifetime;

	C4DynamicParticleProperties properties;
public:
	float GetAge() { return startingLifetime - lifetime; }
	float GetLifetime() { return lifetime; }
	float GetRelativeAge() { return 1.0f - (lifetime / startingLifetime);}

	void Init();
	C4DynamicParticle() { Init(); }

	void SetPosition(float x, float y)
	{
		positionX = x;
		positionY = y;
		drawingData.SetPosition(positionX, positionY, properties.size.GetValue(this),  properties.rotation.GetValue(this));
	}

	bool Exec(C4Object *obj, float timeDelta, C4ParticleDef *sourceDef);

	friend class C4DynamicParticleProperties;
	friend class C4DynamicParticleValueProvider;
	friend class C4DynamicParticleChunk;
	friend class C4DynamicParticleSystem;
};

class C4DynamicParticleChunk
{
private:
	C4ParticleDef *sourceDefinition;
	int blitMode;

	std::vector<C4DynamicParticle*> particles;
	std::vector<C4DynamicParticle::DrawingData::Vertex> vertexCoordinates;
	int particleCount;

	void ReplaceParticle(int indexTo, int indexFrom);
public:
	C4DynamicParticleChunk() : sourceDefinition(0), blitMode(0), particleCount(0)
	{

	}
	~C4DynamicParticleChunk()
	{
		Clear();
	}
	// removes all particles
	void Clear();
	bool Exec(C4Object *obj, float timeDelta);
	void Draw(C4TargetFacet cgo, C4Object *obj);
	bool IsOfType(C4ParticleDef *def, int _blitMode);

	C4DynamicParticle *AddNewParticle();

	friend class C4DynamicParticleList;
};

class C4DynamicParticleList
{
private:
	std::list<C4DynamicParticleChunk> particleChunks;

	C4Object *targetObject;

	// caching..
	C4DynamicParticleChunk *lastAccessedChunk;

	// for making sure that the list is not drawn and calculated at the same time
	CStdCSec accessMutex;
public:
	C4DynamicParticleList(C4Object *obj = 0) : targetObject(obj), lastAccessedChunk(0)
	{

	}
	// deletes all the particles
	void Clear();

	void Exec(float timeDelta = 1.f);
	void Draw(C4TargetFacet cgo, C4Object *obj);
	C4DynamicParticle *AddNewParticle(C4ParticleDef *def, int blitMode);
};

class C4DynamicParticleSystem
{
	class CalculationThread : public StdThread
	{
	protected:
		virtual void Execute();
	public:
		CalculationThread() { StdThread::Start(); }
	};

private:
	std::vector<uint32_t> primitiveRestartIndices;
	std::list<C4DynamicParticleList> particleLists;

	CalculationThread calculationThread;
	CStdCSec particleListAccessMutex;
	int currentSimulationTime; // in game time

	// calculates the physics in all of the existing particle lists
	void ExecuteCalculation();

	C4DynamicParticleList *globalParticles;
public:
	C4DynamicParticleSystem()
	{
		currentSimulationTime = 0;
		globalParticles = 0;
	}

	void Clear();
	void DrawGlobalParticles(C4TargetFacet cgo) { if (globalParticles) globalParticles->Draw(cgo, 0); } 

	C4DynamicParticleList *GetNewParticleList(C4Object *forTarget = 0);
	// releases up to 2 lists
	void ReleaseParticleList(C4DynamicParticleList *first, C4DynamicParticleList *second = 0);

	void PreparePrimitiveRestartIndices(int forSize);
	void *GetPrimitiveRestartArray() { return (void*)&primitiveRestartIndices[0]; }

	C4DynamicParticle *Create(C4ParticleDef *of_def, float x, float y, C4DynamicParticleValueProvider speedX, C4DynamicParticleValueProvider speedY, C4DynamicParticleValueProvider size, float lifetime, C4PropList *properties, C4DynamicParticleList *pxList=NULL, C4Object *object=NULL);

	friend class CalculationThread;
};

extern C4DynamicParticleSystem DynamicParticles;

#endif