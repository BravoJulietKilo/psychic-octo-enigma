// Copyright 2025 Celsian Pty Ltd


#include "PraxisRandomService.h"
#include "Math/UnrealMathUtility.h"

// Combine integers into a stable 32-bit seed (FNV-1a style mix + GetTypeHash)
static inline uint32 MixSeed(uint32 A, uint32 B)
{
	uint32 x = A ^ 0x811C9DC5u;
	x ^= B;
	x *= 16777619u;
	x ^= (x >> 13);
	x *= 0x85EBCA6Bu;
	x ^= (x >> 16);
	return x;
}

void UPraxisRandomService::Initialise(int32 InBaseSeed)
{
	BaseSeed = InBaseSeed;
	TickCount = 0;
	Stateful.Initialize(BaseSeed);
}

int32 UPraxisRandomService::GenerateRandomInt(int32 Min, int32 Max)
{
	return Stream.RandRange(Min, Max);
}

float UPraxisRandomService::GenerateUniformProbability(float Min, float Max)
{
	const float U = Stream.FRand(); // deterministic
	return FMath::Lerp(Min, Max, U);
}

double UPraxisRandomService::SampleExponential(double Lambda)
{
	check(Lambda > 0.0);
	const float U = FMath::Clamp(Stream.FRand(), KINDA_SMALL_NUMBER, 1.0f - KINDA_SMALL_NUMBER);
	return -FMath::Loge(U) / Lambda;
}

float UPraxisRandomService::GenerateExponentialFromMean(float Mean)
{
	check(Mean > 0.0f);
	const double Lambda = 1.0 / Mean;
	return static_cast<float>(SampleExponential(Lambda));
}

bool UPraxisRandomService::EventOccursInStep(float Lambda, float DeltaT)
{
	if (Lambda <= 0.0f || DeltaT <= 0.0f) return false;

	const double p = 1.0 - FMath::Exp(-Lambda * DeltaT);
	const float U = Stream.FRand();
	return U < p;
}

float UPraxisRandomService::GenerateExponentialProbability(float a, float b)
{
	return 1.0f;
}


void UPraxisRandomService::BeginTick(int32 InTickCount)
{
	TickCount = InTickCount;
}

// ------------ Stateless, order-independent draws --------------

FRandomStream UPraxisRandomService::MakeDerivedStream(const FName& Key, int32 Channel) const
{
	// Derive from: BaseSeed, TickCount, Key (hashed), Channel
	uint32 S = static_cast<uint32>(BaseSeed);
	S = MixSeed(S, static_cast<uint32>(TickCount));
	S = MixSeed(S, static_cast<uint32>(GetTypeHash(Key)));
	S = MixSeed(S, static_cast<uint32>(Channel));
	FRandomStream DerivedStream(static_cast<int32>(S));
	return DerivedStream;
}

int32 UPraxisRandomService::RandomInt_Key(const FName& Key, int32 Channel, int32 Min, int32 Max)
{
	FRandomStream R = MakeDerivedStream(Key, Channel);
	return R.RandRange(Min, Max);
}

float UPraxisRandomService::Uniform_Key(const FName& Key, int32 Channel, float Min, float Max)
{
	FRandomStream R = MakeDerivedStream(Key, Channel);
	const float U = R.FRand();
	return FMath::Lerp(Min, Max, U);
}

double UPraxisRandomService::SampleExponential(FRandomStream& Rng, double Lambda)
{
	check(Lambda > 0.0);
	const float U = FMath::Clamp(Rng.FRand(), KINDA_SMALL_NUMBER, 1.0f - KINDA_SMALL_NUMBER);
	return -FMath::Loge(U) / Lambda;
}

float UPraxisRandomService::ExponentialFromMean_Key(const FName& Key, int32 Channel, float Mean)
{
	check(Mean > 0.0f);
	FRandomStream R = MakeDerivedStream(Key, Channel);
	return static_cast<float>(SampleExponential(R, 1.0 / static_cast<double>(Mean)));
}

bool UPraxisRandomService::EventOccursInStep_Key(const FName& Key, int32 Channel, float Lambda, float DeltaT)
{
	if (Lambda <= 0.0f || DeltaT <= 0.0f) return false;
	FRandomStream R = MakeDerivedStream(Key, Channel);
	const double p = 1.0 - FMath::Exp(-static_cast<double>(Lambda) * static_cast<double>(DeltaT));
	return (R.FRand() < p);
}