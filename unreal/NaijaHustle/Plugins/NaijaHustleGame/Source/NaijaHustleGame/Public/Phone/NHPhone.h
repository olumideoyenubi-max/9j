#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "NHPhone.generated.h"

class ANHVehicle;
class ANHPerson;

USTRUCT()
struct FNHPhoneMessage
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FName From;
	UPROPERTY(SaveGame) FString Text;
	UPROPERTY(SaveGame) bool bMe = false;
	UPROPERTY(SaveGame) bool bRead = false;
};

USTRUCT()
struct FNHPhonePost
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FString Author;
	UPROPERTY(SaveGame) FString Handle;
	UPROPERTY(SaveGame) FString Text;
	UPROPERTY(SaveGame) int32 Likes = 0;
	UPROPERTY(SaveGame) bool bMe = false;
};

/** What the phone keeps between sessions (slot "NaijaHustlePhone") */
UCLASS()
class UNHPhoneSave : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) TMap<FName, int32> Friendship;
	UPROPERTY(SaveGame) TArray<FNHPhoneMessage> Chats;
	UPROPERTY(SaveGame) TArray<FNHPhonePost> Feed;
	UPROPERTY(SaveGame) TArray<FString> MissedCalls;
	UPROPERTY(SaveGame) float DriverRating = 5.f;
	UPROPERTY(SaveGame) int32 DriverTrips = 0;
	UPROPERTY(SaveGame) int32 RiderTrips = 0;
};

/**
 * The player's phone: the browser game's four apps (Gist chats, KoboPay, Yarns, MapAm) plus contacts and calls, and the
 * ride-hailing app DropAm, as rider and as driver. One is spawned by the game mode; the HUD draws it (NHHUDPhone.cpp)
 * and ANHPlayerController sends it the keys (P opens it; arrows, Enter and Backspace work it).
 *
 * The phone is a stack of pages, each a title and a list of rows; a row has what Enter does and, for some, what Left
 * and Right change. Calls are lines of dialogue shown as subtitles (there are no voice recordings), then the
 * contact's options. Everything named here is invented: the apps, the people, the companies.
 *
 * DropAm as rider: pick where to (the map's pin, or a stop) and what kind of ride; a real vehicle with a driver is
 * made on the road nearby and drives to you along the road graph; get in with F and it drives you there (Enter skips
 * the trip). Fares follow the road distance, with a surge in the rain and at rush hour. Drivers sometimes cancel, say
 * "I dey come", or ask you to walk to them. As driver: go online while driving your own car and take pickup and
 * drop-off jobs for a fare, a tip and a rating. Needs the real city's road graph (UNHGameData::bRealCity).
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHPhone : public AActor
{
	GENERATED_BODY()

public:
	ANHPhone();
	static ANHPhone* Get(const UObject* WorldContext);
	virtual void Tick(float DeltaSeconds) override;

	// ---- what the HUD draws
	struct FRow
	{
		FString Text;
		FString Detail;
		FLinearColor Color = FLinearColor(0.96f, 0.95f, 0.9f);
		/** A coloured tile with this letter before the text: a contact's portrait or an app's icon */
		FString Badge;
		FLinearColor BadgeColor = FLinearColor::Black;
		/** 0..100 draws a friendship bar under the row; -1 none */
		int32 Meter = -1;
		bool bChoice = true;
		TFunction<void()> Do;
		TFunction<void(int32)> Change;
	};
	FString Title;
	FString Footer;
	TArray<FRow> Rows;
	int32 Selected = 0;
	/** On a page with nothing to choose (a chat, the feed), how many rows down it is scrolled */
	int32 Scroll = 0;
	/** The line being spoken on a call or by a driver, for the subtitle strip */
	FString SubtitleSpeaker, Subtitle;
	/** Somebody is ringing: their name ("" if nobody) */
	FString Ringing() const;
	/** Unread chats plus missed calls plus unseen Yarns posts: the HUD's phone icon */
	int32 Unseen() const;
	bool IsOpen() const { return bOpen; }
	/** Where the hailed ride or the driver job is, for the maps; false if there is none */
	bool RideMarker(FVector2D& Out, FString& Label) const;

	// ---- keys
	void Toggle();
	void Close() { bOpen = false; }
	void Move(int32 Dir);
	void Change(int32 Dir);
	void Select();
	void Back();
	/** F: gets into a hailed ride that is waiting within reach, or out of one mid-trip; false if there is nothing of the kind to do */
	bool Interact();
	/** What F does for the phone's ride right now ("" if nothing) */
	FString InteractPrompt() const;
	bool IsRiding() const { return Ride.Stage == ERide::Trip; }
	/** A call is being spoken (not just ringing): the audio mix ducks everything else */
	bool InCall() const { return Call.bActive && !Call.bIncoming; }

	/** For scripted screenshots: "phone", "contacts", "call", "dropam" (orders a car to Yaba), "ride" (the same, and gets in when it comes), "driver" */
	void DebugOpen(const FString& What);

	// ---- for the rest of the game
	void Message(FName From, const FString& Text);
	void Post(const FString& Author, const FString& Handle, const FString& Text, bool bAboutMe);
	int32 FriendshipOf(FName Contact) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	enum class EPage : uint8 { Home, Gist, Thread, Kobo, Yarns, Contacts, Contact, HangOut, DropAm, DropAmDriver, Missed, Music };
	struct FContact
	{
		FName Id;
		FString Name, Role;
		FLinearColor Color;
		bool bFriend = false;
		TArray<FString> Hello;
	};
	TArray<FContact> Contacts;
	const FContact* Find(FName Id) const;
	bool bOpen = false;
	TArray<EPage> Pages;
	FName PageContact;
	void Go(EPage Page);
	void Build();
	FRow& Row(const FString& Text, const FString& Detail = FString(), TFunction<void()> Do = nullptr);
	void Info(const FString& Text, const FLinearColor& Color = FLinearColor(0.7f, 0.68f, 0.62f));

	// ---- saved state
	UPROPERTY() TObjectPtr<UNHPhoneSave> State;
	void Save();
	void AddFriendship(FName Contact, int32 Amount);
	int32 UnseenPosts = 0;

	// ---- calls
	struct FCall
	{
		FName Who;
		bool bIncoming = false;
		float RingLeft = 0.f;
		TArray<FString> Lines;
		int32 Line = 0;
		float LineLeft = 0.f;
		TFunction<void()> After;
		bool bActive = false;
	};
	FCall Call;
	float NextIncoming = 150.f;
	void Speak(FName Who, const TArray<FString>& Lines, TFunction<void()> After = nullptr);
	void CallContact(FName Who);
	void ContactOptions(const FContact& C);
	void HangOut(const FString& What, int32 Cost, int32 Gain, float Hours);
	FString Me() const { return TEXT("You"); }

	// ---- KoboPay
	int32 KoboTo = 0;
	int32 KoboAmount = 1000;
	FString KoboNote;

	// ---- DropAm, as rider
	enum class ERide : uint8 { None, Finding, Coming, Waiting, Trip, Leaving };
	struct FRide
	{
		ERide Stage = ERide::None;
		TWeakObjectPtr<ANHVehicle> Car;
		TArray<FVector2D> Line;
		TArray<float> LineAt;   // distance along the line at each point
		float Along = 0.f, Speed = 0.f, Timer = 0.f, ChatLeft = 0.f, Yaw = 0.f;
		FVector2D At = FVector2D::ZeroVector;
		FVector2D Dest = FVector2D::ZeroVector;
		FString DestName, Driver;
		FName Type;
		int32 Fare = 0, Chat = 0;
		float TripLength = 1.f;
		bool bFriend = false;
		bool bExcused = false;
		bool bStopEarly = false;
		bool bAutoBoard = false;
	};
	FRide Ride;
	int32 RideType = 2;
	int32 RideDest = -1;   // -1 the map's pin, otherwise an index into the stops
	bool RideDestination(FVector2D& Out, FString& Name) const;
	int32 FareFor(int32 Type, float RoadLength, float& OutSurge) const;
	void RequestRide(bool bFromFriend, FName Friend = NAME_None);
	bool SendCar(const FVector2D& To, float FromNear, float FromFar);
	void SetLine(const TArray<FVector2D>& Line);
	/** Carries the ride's car along its line; true when it has reached the end */
	bool Drive(float DeltaSeconds, float Cruise);
	void TickRide(float DeltaSeconds);
	void Board();
	void EndTrip(bool bArrived);
	void CancelRide(const FString& Why);

	// ---- DropAm, as driver
	enum class EJob : uint8 { Offline, Waiting, Offer, ToPickup, ToDropoff };
	struct FJob
	{
		EJob Stage = EJob::Offline;
		float Timer = 0.f, Started = 0.f, Expected = 1.f;
		FVector2D Pickup = FVector2D::ZeroVector, Dropoff = FVector2D::ZeroVector;
		FString Passenger, Where, From;
		int32 Fare = 0;
		TWeakObjectPtr<ANHPerson> Body;
		float WorstBump = 0.f, LastSpeed = 0.f;
	};
	FJob Job;
	void TickJob(float DeltaSeconds);
	void OfferJob();
	void EndJob(bool bDone);
	ANHVehicle* PlayerCar() const;
	class ANHHUD* Hud() const;
	bool Raining() const;
	bool RushHour() const;
};
