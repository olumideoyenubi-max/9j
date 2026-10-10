#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NHEstate.generated.h"

class ANHVehicle;

/** A line of the place menu, as the HUD's list menu draws it */
struct FNHMenuLine
{
	FString Label, Value, Help;
};

/** A place as a website lists it */
struct FNHPlaceCard
{
	FName Id;
	FString Kind, Name, Area, About;
	int64 Price = 0;
	int32 Fee = 0;
	bool bOwned = false;
	FVector2D Where = FVector2D::ZeroVector;
	/** Whose home it is, of the people to play ("" if nobody's) */
	FString Resident;
	bool bIsHome = false;
};

/** A kept car as the phone lists it */
struct FNHGarageCard
{
	int32 Serial = 0;
	FString Name, HomeName;
	bool bWrecked = false, bOut = false;
	float Fuel = 1.f;
	int64 Value = 0;
};

/**
 * Land, houses and businesses in the real-scale city, and the rich people who can be played (Data/estate.json).
 *
 *  - A place stands on the verge of the nearest main road to where the data puts it: a board for land, a house or a
 *    flat for sale; a lit doorway for a bar, a night club or a strip club; a canopy and pumps for a filling station.
 *    It is only built while the player is within a few hundred metres.
 *  - E at a place opens its menu (ANHHUD draws it with the list menu): buy it, sell it, rest there, make it home;
 *    go into a bar or a club and buy at the counter; fill the tank.
 *  - A bar or a club is gone into: its room is built under the ground beneath its door when the player goes in and
 *    taken down when they come out, one plan a kind of place with the furniture varied by the place's name.
 *  - People: PlayAs puts the player in that person's body at their home, with their bank balance, their home in
 *    their name and their cars outside. Console: NHPlayAs <id>, NHWho (or N); command line: -NHPlayAs=<id>.
 *
 * Money goes through UNHHustleSubsystem (Pay takes from the pocket, then the bank). Single player for now: what is
 * owned lives in the host's subsystem and save.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHEstate : public AActor
{
	GENERATED_BODY()

public:
	ANHEstate();
	static ANHEstate* Get(const UObject* WorldContext);
	virtual void Tick(float DeltaSeconds) override;

	/** What E would do here, for the HUD's prompt; empty when nowhere near a place */
	FString Prompt(const APawn* Pawn) const;
	/** E: opens the menu of the place the player is at (or is inside). False when there is none. */
	bool Interact(APawn* Pawn);

	bool MenuOpen() const { return Mode != EMode::None; }
	void Menu(FString& OutTitle, FString& OutHeading, TArray<FNHMenuLine>& OutLines) const;
	void Choose(int32 Line);
	void CloseMenu() { Mode = EMode::None; }
	/** The list of people to play */
	void OpenPeople();
	bool PlayAs(FName Id);
	/** Puts the player at a place's door: NHPlace <id> */
	bool GoTo(FName Id);
	/** Quick travel to a place the player owns: there at once, the clock moved on by the drive it would have been */
	bool Travel(FName Id);
	bool IsInside() const { return Inside != INDEX_NONE; }
	/**
	 * Garages. A home keeps the player's cars: six at a big house or flat, two at a small one. A kept car stands
	 * outside its home, is saved with the game, can be brought to wherever the player is by the mechanic, and can be
	 * claimed on the insurance when it has been wrecked.
	 */
	void GarageCards(TArray<FNHGarageCard>& Out) const;
	/** Keeps a car at a home (the player's home if none is said). False if there is no home or no room. */
	bool Keep(ANHVehicle* Car, FName Home = NAME_None);
	/** The mechanic brings a kept car to the road by the player, for a fee */
	bool Deliver(int32 Serial);
	/** Insurance: a wrecked kept car made whole for a premium */
	bool Claim(int32 Serial);
	/** Out of the garage for good, for half what it is worth */
	bool SellCar(int32 Serial);
	static constexpr int32 DeliveryFee = 25000;
	/** Every place, for the phone's websites */
	void Cards(TArray<FNHPlaceCard>& Out) const;
	int32 PetrolPrice() const { return PetrolPerLitre; }
	/** For the log: how many places, how many built, what is owned */
	FString Describe() const;

protected:
	virtual void BeginPlay() override;

private:
	enum class EMode : uint8 { None, Place, People };
	struct FPlace
	{
		FName Id;
		FString Kind, Name, Area, About;
		FVector2D Want = FVector2D::ZeroVector;
		FVector At = FVector::ZeroVector;      // on the verge, on the ground
		float Yaw = 0.f;                        // facing the road
		int64 Price = 0;
		int32 Fee = 0;
		bool bPlaced = false;
		TWeakObjectPtr<AActor> Built;
	};
	struct FPerson
	{
		FName Id, Skin, Home;
		FString Name, About;
		int64 Bank = 0;
		TArray<FName> Cars;
	};
	struct FDrink
	{
		FString Name;
		int32 Price = 0;
		float Heal = 0.f;
	};
	TArray<FPlace> Places;
	TArray<FPerson> People;
	TMap<FString, TArray<FDrink>> Menus;
	int32 Pocket = 5000000, PetrolPerLitre = 900;
	EMode Mode = EMode::None;
	int32 Shown = INDEX_NONE;       // the place whose menu is open
	int32 Near = INDEX_NONE;        // the place the player is standing at
	int32 Inside = INDEX_NONE;      // the bar or club the player is in
	FVector Outside = FVector::ZeroVector;
	UPROPERTY() TObjectPtr<AActor> Room;
	UPROPERTY() TArray<TObjectPtr<ANHVehicle>> Garage;
	float Look = 0.f;
	FName StartAs;

	bool Load();
	bool Place(FPlace& P) const;
	void Build(FPlace& P);
	void BuildRoom(const FPlace& P);
	void BuildHome(const FPlace& P, AActor* Holder, USceneComponent* Base);
	/** The pool and the shower of a home, built beside its living room; the player is taken there to swim or wash */
	void BuildWetRoom(AActor* Holder, USceneComponent* Base);
	const FVector WetRoom = FVector(6000.f, 0.f, 0.f);
	bool bAway = false;
	FVector AwayFrom = FVector::ZeroVector;
	/** The room model for a kind of home, if it has been brought in (/Game/Interiors) */
	class UStaticMesh* HomeModel(const FPlace& P) const;
	/** Somewhere that can be gone into: a bar or a club, and a house or a flat that is the player's own */
	bool Enterable(const FPlace& P) const { return IsVenue(P) || (IsHome(P) && Owns(P)); }
	FName StartHome;
	void LeaveRoom(APawn* Pawn);
	bool Owns(const FPlace& P) const;
	bool IsHome(const FPlace& P) const { return P.Kind == TEXT("house") || P.Kind == TEXT("apartment"); }
	bool IsVenue(const FPlace& P) const { return P.Kind == TEXT("bar") || P.Kind == TEXT("club") || P.Kind == TEXT("strip"); }
	ANHVehicle* CarAt(const FPlace& P, const APawn* Pawn) const;
	int32 Slots(const FPlace& P) const { return !IsHome(P) ? 0 : P.Price >= 500000000 ? 6 : 2; }
	int32 KeptAt(FName Home) const;
	/** Stands the kept cars of a home outside it that are not already about */
	void Park(const FPlace& P);
	ANHVehicle* Standing(int32 Serial) const;
	ANHVehicle* Make(struct FNHGarageCar& Record, const FTransform& Where);
	bool Kerb(FTransform& Out) const;
	float GarageLook = 0.f;
	FVector RoomAt(const FPlace& P) const { return P.At + FVector(0.f, 0.f, -6000.f); }
};
