#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "NHPlayerController.generated.h"

class UNHInputSet;
class ANHVehicle;
class ANHCharacter;
class UNHDebugPlay;

UENUM(BlueprintType)
enum class ENHInputMode : uint8
{
	OnFoot,
	Vehicle,
	Menu
};

/**
 * Owns the input set and decides which mapping contexts are active (on foot, driving, in a menu).
 * Gets the player in and out of vehicles (F), sends E and the number keys to the game director
 * (calls, choices, dialogue) and builds the on-screen prompt.
 */
UCLASS()
class NAIJAHUSTLEGAME_API ANHPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Built on first use, so pawns can bind to it from SetupPlayerInputComponent whatever the init order. */
	UNHInputSet* GetInputSet();

	/** Swaps the active mapping contexts. Global stays on in every mode. */
	UFUNCTION(BlueprintCallable, Category = "Naija|Input")
	void SetNHInputMode(ENHInputMode NewMode);

	UFUNCTION(BlueprintPure, Category = "Naija|Input")
	ENHInputMode GetNHInputMode() const { return InputMode; }

	/** Console: NHLighting Day | DustyNoon | Sunset | NightRain | HarshMorning | GoldenEvening (or no argument for the next one) */
	UFUNCTION(Exec)
	void NHLighting(const FString& PresetName);

	/** Console: NHLightMenu (or F1) opens the lighting debug menu */
	UFUNCTION(Exec)
	void NHLightMenu();

	/** Console: NHReset wipes the save and starts the story again (reload the level after) */
	UFUNCTION(Exec)
	void NHReset();

	/** Console: NHCash 50000 */
	UFUNCTION(Exec)
	void NHCash(int32 Amount);

	/** Console: NHTime 9.5 sets the clock to 9:30 today (the lighting follows) */
	UFUNCTION(Exec)
	void NHTime(float Hour);
	/** What you carry, in the log */
	UFUNCTION(Exec) void NHBag();
	/** The story's missions (Data/missions): NHMission m02 starts one, NHObjective passes the objective in hand, NHMissionAbort gives it up, NHCheckpoint goes back to the last checkpoint */
	UFUNCTION(Exec) void NHMission(const FString& Id);
	UFUNCTION(Exec) void NHObjective();
	UFUNCTION(Exec) void NHMissionAbort();
	UFUNCTION(Exec) void NHCheckpoint();
	/** The story's cast: NHSwitch goes to the other lead, NHSwitch amaka to one by id (Tab does the first). NHAbility is the Z key. */
	UFUNCTION(Exec) void NHSwitch(const FString& Who);
	UFUNCTION(Exec) void NHAbility();
	/** NHMeter 1 fills the ability meter of whoever is being played; NHFlag spared_jaguar 1 sets one of the story's flags */
	UFUNCTION(Exec) void NHMeter(float Value);
	UFUNCTION(Exec) void NHFlag(const FString& Name, int32 Value);
	/** Somebody rich to be: NHPlayAs chief, NHPlayAs madam (Data/estate.json). NHWho lists them on the screen. */
	UFUNCTION(Exec) void NHPlayAs(const FString& Who);
	UFUNCTION(Exec) void NHWho();
	/** To a place's door: NHPlace club_vi. NHPlaceUse 0 chooses a line of its menu (opening it first). */
	UFUNCTION(Exec) void NHPlace(const FString& Id);
	UFUNCTION(Exec) void NHPlaceUse(int32 Line);
	/** Anywhere on the map, by its place in centimetres: NHAt 282051 349417 stands the player on the ground there */
	UFUNCTION(Exec) void NHAt(float X, float Y);
	/** Puts things in the bag: NHGive suya 3. Single player and the host only. */
	UFUNCTION(Exec) void NHGive(const FString& Item, int32 HowMany);

	/**
	 * Console: NHLookShots X Y Yaw [Folder]. Look-development screenshots: puts you on foot at X, Y (cm) facing Yaw at
	 * 9:30, stands still for 15 s to measure the frame rate (logged), saves street.png, then sprints forward and saves
	 * sprint.png. Folder defaults to Saved/Screenshots/NaijaLook.
	 * Also runs from the command line: -NHLookShots=X,Y,Yaw -NHLookDir=Folder (and quits when done); add -NHLookHour=17
	 * for another time of day.
	 */
	UFUNCTION(Exec)
	void NHLookShots(float X, float Y, float Yaw, const FString& Folder);

	// ---- playtest commands (they do nothing in Shipping builds); see UNHDebugPlay
	/** Console: NHGoto <stop id | park | bay1> puts you, or the vehicle you are driving, there, facing along the road */
	UFUNCTION(Exec)
	void NHGoto(const FString& Where);
	/** Console: NHBoard calls passengers at the current stop until nobody waits, giving the right change every time */
	UFUNCTION(Exec)
	void NHBoard();
	/** Console: NHAgbero pay | beg | drive answers the agbero if his panel is open */
	UFUNCTION(Exec)
	void NHAgbero(const FString& What);
	/** Console: NHFinish completes the current mission objective */
	UFUNCTION(Exec)
	void NHFinish();
	/** Console: NHAutoplay plays "First Day on the Danfo" by script and checks the money (starts the story again if it is done) */
	UFUNCTION(Exec)
	void NHAutoplay();
	/** Console: NHSelfTest checks getting in and out of every vehicle, a missed stop, a wrecked bus and the deadline */
	UFUNCTION(Exec)
	void NHSelfTest();
	/** Console: NHPaintDemo X Y stands four test bodies there wearing the car paint: clean, crashed, wet in the rain, and cracked glass */
	UFUNCTION(Exec)
	void NHPaintDemo(float X, float Y);
	/** Console: NHDriveShots Type Folder gets into the first vehicle of that type and saves a picture of the driver by day and one of the headlights at night */
	UFUNCTION(Exec)
	void NHDriveShots(const FString& Type, const FString& Folder);
	/** Console: NHSkinShots Folder saves a front picture of the player's body and one of the face */
	UFUNCTION(Exec)
	void NHSkinShots(const FString& Folder);
	/** Console: NHHeadlights switches the headlights of the vehicle you are driving (the K key) */
	UFUNCTION(Exec)
	void NHHeadlights();
	/** Console: NHSkin changes the player's body to the next skin the project has; NHSkin <id> picks one (naija, hustler, mannequin). The choice is remembered. */
	UFUNCTION(Exec)
	void NHSkin(const FString& Id);
	/** Console: NHWear hair|top|bottom|shoes [steps] puts on the next piece in that slot; NHWear topcolour|bottomcolour|shoecolour [steps] the next colour. For a character with a wardrobe; saved like the Clothes page. */
	UFUNCTION(Exec)
	void NHWear(const FString& What, int32 Steps = 1);
	/** Console: NHPeople [count] [distance] stands that many passers-by in rows that far in front of the player (behind, if negative), each different, for two minutes */
	UFUNCTION(Exec)
	void NHPeople(int32 Count = 8, float Distance = 350.f);
	/** Console: NHCarShow X Y [Folder] lines up one of every vehicle type there, facing +X (east on the map), to check real models; with a full folder path it saves pictures from above and from in front of each pair */
	UFUNCTION(Exec)
	void NHCarShow(float X, float Y, const FString& Folder);

	/** Console: NHAudio prints the audio mix: classes, sliders, the mixes that are on, the space, the voices in use */
	UFUNCTION(Exec)
	void NHAudio();
	/** Console: NHAudioSpace Street|Market|MotorPark|Interior|UnderBridge|Tunnel holds that reverb and EQ; NHAudioSpace auto gives it back to where you stand */
	UFUNCTION(Exec)
	void NHAudioSpace(const FString& Name);
	/** Console: NHResponse prints what kind of area this is, the nearest station, and who is coming for you */
	UFUNCTION(Exec)
	void NHResponse();
	/** Console: NHRadio (next station, off after the last), NHRadio track (next song), NHRadio off. For the car you are driving, or on foot the nearest one, which you then hear from outside. */
	UFUNCTION(Exec)
	void NHRadio(const FString& What);
	/** Console: NHAudioTest plays the test tones through the mix and records them (see ANHAudioTest) */
	UFUNCTION(Exec)
	void NHAudioTest();
	/** Mouse and stick look speed, 1 as built (the pause menu's setting) */
	float LookScale = 1.f;
	/** How far the city is loaded from the player right now, cm, and whether the streaming overlay is showing (F2) */
	float StreamingRadius = 45000.f;
	bool bStreamingOverlay = false;
	/** The on-foot character, also while the player drives */
	ANHCharacter* GetOnFootCharacter() const { return OnFootCharacter; }
	/** The vehicle the player last drove, if it is still around */
	ANHVehicle* GetLastVehicle() const { return LastVehicle.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Naija|Vehicle") bool EnterVehicle(ANHVehicle* Vehicle);
	/** Steps out beside the vehicle. Refuses above walking pace unless bForce. */
	UFUNCTION(BlueprintCallable, Category = "Naija|Vehicle") bool LeaveVehicle(bool bForce = false);
	/** The nearest vehicle you can get into from here, if any */
	ANHVehicle* NearbyVehicle() const;
	/** The bottom-of-screen prompt for what F and E do right now */
	FString Prompt() const;

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;

private:
	friend class UNHDebugPlay;
	UNHDebugPlay* DebugPlay();
	UPROPERTY(Transient)
	TObjectPtr<UNHDebugPlay> Debug;

	// the map (M), the pause menu (Esc or P) and the inventory wheel (hold Tab): keys go to the HUD, which draws them
	void UiMap();
	void UiMenu();
	void UiPhone();
	void OnRunToggle();
	void UiRadioClose();
	/** F2: World Partition's own map of loaded cells, and a line saying what is loaded */
	void OnStreamingOverlay();
	/** World Partition: how much of the city is loaded round the player, wider the faster they drive */
	void UpdateStreaming();
	TWeakObjectPtr<ANHVehicle> StreamingCar;
	void OnRoll();
	void OnCrouch();
	void OnBag();
	/** -NHBagTest: eats, fires till the rounds are gone, drops, and photographs the bag: Saved/NHBag/bag.png */
	void BagTestStep(int32 Step);
	void OnClimb();
	void OnFire();
	void OnAimEnd();
	/** -NHEstateTest: the rich to play, a club and a strip club gone into, land bought, the pumps: pictures in Saved/NHEstate/ */
	void EstateTestStep(int32 Step);
	/** -NHRideTest=okada: that vehicle ridden off, turned and braked, its wheels, lean and rider logged: pictures in Saved/NHRide/ */
	void RideTestStep(int32 Step);
	UPROPERTY() TObjectPtr<class ANHVehicle> RideTestCar;
	/** -NHActionTest: the action clips one after another, stood and then walking, each photographed from in front: Saved/NHActions/ */
	void ActionTestStep(int32 Step);
	UPROPERTY() TObjectPtr<class ACameraActor> ActionLens;
	/** -NHActionTest: the way the body is being walked, zero while it stands; and the line it walks along */
	FVector ActionWalk = FVector::ZeroVector, ActionAhead = FVector::ForwardVector;
	void OnFireEnd();
	void UiBack();
	void UiWheelOpen();
	/** Tab: let go quickly, it switches lead; held a quarter of a second, it opens the inventory wheel */
	void OnTabDown();
	void OnTabUp();
	FTimerHandle TabHold;
	bool bTabWheel = false;
	void OnAbility();
	void UiWheelClose();
	void UiUp();
	void UiDown();
	void UiLeft();
	void UiRight();
	void UiAccept();
	void UiClick();
	void UiRightClick();
	void UiZoomIn();
	void UiZoomOut();
	TWeakObjectPtr<ANHVehicle> LastVehicle;
	bool bToldLights = false;
	/** -NHRadioTest: gets into the nearest car, turns the radio on, gets out, skips a song, recording it all to Saved/NHAudio/nh_radio_test.wav, and quits */
	void RadioTestStep(int32 Step);
	void WeaponTestStep(int32 Step);
	void DamageTestStep(int32 Step);
	void ResponseTestStep(int32 Step);
	void UiClickEnd();

	void OnCycleLighting();
	void OnInteract();
	void OnAction();
	void OnChoice1() { Choose(0); }
	void OnChoice2() { Choose(1); }
	void OnChoice3() { Choose(2); }
	void OnChoice4() { Choose(3); }
	void Choose(int32 Index);

	/** The on-foot character, kept while you drive */
	UPROPERTY(Transient)
	TObjectPtr<ANHCharacter> OnFootCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UNHInputSet> InputSet;

	ENHInputMode InputMode = ENHInputMode::OnFoot;

	void LookShot(const TCHAR* Name);
	void LookShotsSprint();
	void LookShotsDone();
	FString LookShotFolder;
	FTimerHandle LookShotTimer;
	bool bLookShotSprint = false;
	/** How far one foot gets ahead of and behind the other during the look-shot sprint, cm: a body whose legs do not move shows next to nothing */
	float LookStrideMin = 0.f, LookStrideMax = 0.f;
	/** While the shots run the view is held on this yaw, whatever the mouse does */
	bool bLookShotActive = false;
	bool bLookFps = false;
	float LookShotHour = 9.5f;
	int32 LookFpsFrames = 0;
	double LookFpsSeconds = 0.0;
	float LookShotYaw = 0.f;
	bool bLookShotQuit = false;
};
