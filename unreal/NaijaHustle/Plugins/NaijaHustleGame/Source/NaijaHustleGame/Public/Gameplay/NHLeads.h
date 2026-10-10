#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHLeads.generated.h"

class ANHCharacter;
class ANHPerson;
class ANHVehicle;
class ACameraActor;
struct FNHLeadSpot;

/** Something Amaka's Unlock works on that stands in the street: a camera on a pole, or a traffic light */
UENUM()
enum class ENHHackKind : uint8
{
	Camera,
	Light
};

/** A CCTV camera or a traffic light, as blockout pieces. ANHLeads stands a few round the player and takes them away again. */
UCLASS()
class NAIJAHUSTLEGAME_API ANHHackPoint : public AActor
{
	GENERATED_BODY()

public:
	ANHHackPoint();
	void Build(ENHHackKind InKind);
	ENHHackKind GetKind() const { return Kind; }
	/** Where the marker goes: the camera's box, the light's head */
	FVector Head() const;
	/** Unlocked for that long: the camera's lamp goes out, the light shows red */
	void Hack(float Seconds);
	bool IsHacked() const;

private:
	ENHHackKind Kind = ENHHackKind::Camera;
	double HackedUntil = 0.0;
	bool bShownHacked = false;
	UPROPERTY() TObjectPtr<class UStaticMeshComponent> Lamp;
	UPROPERTY() TObjectPtr<class UStaticMeshComponent> Green;
	virtual void Tick(float DeltaSeconds) override;
	void Show(bool bHacked);
};

/**
 * The story's cast (Data/characters.json) and the two leads the player moves between, Tunde and Amaka (docs/STORY.md).
 *
 * Switching (Tab, or Switch): the camera lifts off the lead being left, the player becomes the other one where they
 * were last left (the first time, beside the bus stop of their part of town), and the camera comes down. The lead left
 * behind stays standing where they were; coming back finds them there. A mission can lock switching, or switch for
 * the player with bForce.
 *
 * Abilities (Z, or UseAbility), each on a meter that fills with play. The numbers are naija_rules.json "abilities":
 *   Tunde, Hustle Rush: for a few seconds he runs faster and takes half of every blow; at the wheel the engine pulls
 *     harder, the top speed is higher and the tyres hold better.
 *   Amaka, Unlock: within 30 m she gets into the nearest phone (somebody standing near), CCTV camera or traffic
 *     light, marked on the screen. A phone gives up a little money and costs a point of Integrity; a camera takes a
 *     wanted star off; a light goes red and the traffic stops short of it.
 *
 * Who can be played is the cast's "unlock" rule: from the start, after a mission, after a mission with a flag set,
 * after the story, or bought. The flags and the bought list are on UNHHustleSubsystem and saved.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHLeads : public AActor
{
	GENERATED_BODY()

public:
	ANHLeads();
	static ANHLeads* Get(const UObject* WorldContext);
	virtual void Tick(float DeltaSeconds) override;

	struct FMember
	{
		FName Id, Stop, Skin, Ability;
		FString Name, From, Role, AbilityName, Unlock;
		int32 Age = 0;
		TArray<FString> Districts;
		FLinearColor Colour = FLinearColor::White;
	};
	const TArray<FMember>& Members() const { return Crew; }
	const FMember* Find(FName Id) const { return Crew.FindByPredicate([Id](const FMember& M) { return M.Id == Id; }); }
	/** Who the player is now */
	FName Current() const;
	bool IsUnlocked(FName Id) const;
	bool IsLead(FName Id) const { return Leads.Contains(Id); }

	/** To the other lead (none), or to one of the cast by id. bForce: a mission doing it for the player, whatever the lock. False, with a line on the screen, if it cannot. */
	bool Switch(FName To = NAME_None, bool bForce = false);
	bool IsSwitching() const { return Stage != EStage::Idle; }
	/** A mission holds the player to the lead they are */
	void SetLocked(bool bOn, const FString& Why = FString()) { bLocked = bOn; LockWhy = Why; }
	bool IsLocked() const { return bLocked; }
	/** Where one of the cast was left in this level, or the place they start from */
	FVector SpotOf(FName Id) const;
	bool WasPlaced(FName Id) const;
	/** The body standing where a lead was left, if the player is near enough for there to be one */
	ANHCharacter* StandIn(FName Id) const;

	// ---- abilities
	/** 0..1; the ability can be used at 1 */
	float Meter(FName Id) const;
	void SetMeter(FName Id, float Value);
	/** Z. False, with a line on the screen, if the meter is not full or (Unlock) nothing is in reach. */
	bool UseAbility();
	bool RushOn() const { return RushLeft > 0.f; }
	float RushSecondsLeft() const { return RushLeft; }
	/** What Unlock last got into: phone, camera or light ("" before the first) */
	FName LastUnlocked;
	/** Cameras are down (an unlocked one) until then: nobody watching them can report the player */
	bool CamerasDown() const;
	/** Stands a camera or a light a few metres in front of the player, for tests */
	ANHHackPoint* PlaceHackPoint(ENHHackKind Kind, float Ahead = 900.f);

	/** What the HUD draws: who you are and the meter, the Unlock marker, and the card while switching */
	struct FHud
	{
		FString Name, AbilityName, SwitchTo, SwitchFrom, TargetLabel;
		FLinearColor Colour = FLinearColor::White;
		float Meter = 0.f, RushLeft = 0.f;
		bool bHasAbility = false, bTarget = false, bSwitching = false, bCanSwitch = false;
		FVector TargetAt = FVector::ZeroVector;
	};
	FHud Hud() const;

protected:
	virtual void BeginPlay() override;

private:
	/** Everybody in Data/characters.json (not called Cast: that is the engine's cast function) */
	TArray<FMember> Crew;
	TArray<FName> Leads;
	bool Load();
	FName LevelKey() const;
	FNHLeadSpot* Spot(FName Id, bool bAdd) const;
	FVector HomeOf(const FMember& M) const;
	bool Ground(const FVector2D& At, float& OutZ) const;
	ANHCharacter* Player() const;
	void Wear(ANHCharacter* Body, const FMember& M, bool bPlayer) const;

	// ---- the switch
	enum class EStage : uint8 { Idle, Rise, Land, Drop };
	EStage Stage = EStage::Idle;
	float StageT = 0.f;
	FName SwitchFrom, SwitchTo;
	FVector LandAt = FVector::ZeroVector;
	float LandYaw = 0.f;
	bool bLocked = false;
	FString LockWhy;
	UPROPERTY() TObjectPtr<ACameraActor> Sky;
	void StartRise();
	void DoLand();
	void EndSwitch();
	void Freeze(bool bOn) const;
	bool bStarted = false;
	float StartWait = 0.4f;

	// ---- the leads left standing
	UPROPERTY() TMap<FName, TObjectPtr<ANHCharacter>> StandIns;
	float StandInWait = 0.f;
	void UpdateStandIns();

	// ---- abilities
	float SwitchSeconds = 0.7f, SwitchHeight = 4500.f;
	float RushFill = 120.f, RushSeconds = 8.f, RushSpeed = 1.35f, RushDamage = 0.5f, RushPull = 1.6f, RushTop = 1.15f, RushGrip = 1.3f;
	float UnlockFill = 150.f, UnlockRange = 3000.f, CameraStars = 1.f, CameraSeconds = 25.f, LightSeconds = 15.f, LightRadius = 6000.f;
	int32 PhoneCashMin = 800, PhoneCashMax = 4500, PhoneIntegrity = -1;
	float RushLeft = 0.f;
	TWeakObjectPtr<ANHVehicle> RushCar;
	float RushCarGrip = 0.f;
	void ApplyRush(bool bOn);
	double CamerasDownUntil = 0.0;
	// what Unlock would get into right now
	TWeakObjectPtr<AActor> Target;
	float TargetWait = 0.f;
	void FindTarget();
	UPROPERTY() TArray<TObjectPtr<ANHHackPoint>> HackPoints;
	float HackWait = 1.f;
	void UpdateHackPoints();
};
