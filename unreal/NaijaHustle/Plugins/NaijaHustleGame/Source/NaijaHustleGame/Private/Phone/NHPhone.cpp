#include "Phone/NHPhone.h"

#include "Camera/PlayerCameraManager.h"
#include "Core/NHGameData.h"
#include "Core/NHHustleSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/NHGameDirector.h"
#include "Gameplay/NHPerson.h"
#include "Kismet/GameplayStatics.h"
#include "Lighting/NHLightingRig.h"
#include "NaijaHustleGame.h"
#include "Player/NHCharacter.h"
#include "Player/NHPlayerController.h"
#include "UI/NHHUD.h"
#include "Vehicles/NHTraffic.h"
#include "Vehicles/NHVehicle.h"
#include "World/NHStreets.h"

namespace NHPhoneData
{
	const TCHAR* Slot = TEXT("NaijaHustlePhone");
	const FLinearColor Muted(0.7f, 0.68f, 0.62f);
	const FLinearColor Yellow(1.f, 0.77f, 0.f);
	const FLinearColor Good(0.35f, 0.85f, 0.45f);
	const FLinearColor Bad(1.f, 0.35f, 0.3f);
	const FName DropAm(TEXT("dropam"));

	struct FRideType { const TCHAR* Name; const TCHAR* Vehicle; int32 Base; int32 PerKm; };
	const FRideType RideTypes[] = { { TEXT("Okada"), TEXT("okada"), 300, 150 }, { TEXT("Keke"), TEXT("keke"), 400, 200 }, { TEXT("Car"), TEXT("sedan"), 700, 350 }, { TEXT("Luxury"), TEXT("luxsedan"), 2500, 900 } };
	const TCHAR* DriverNames[] = { TEXT("Tunde"), TEXT("Emeka"), TEXT("Sani"), TEXT("Kayode"), TEXT("Chidi"), TEXT("Musa"), TEXT("Segun"), TEXT("Efe") };
	const TCHAR* DriverChat[] = {
		TEXT("Oga, this go-slow no be here. Na so e be since morning."),
		TEXT("You know say fuel don cost again? Na only God dey help us for this work."),
		TEXT("Abeg, when we reach, give me five star. My rating dey shake."),
		TEXT("My brother drive danfo for Oshodi. E say conductor work no easy."),
		TEXT("See as that keke just enter road. Lagos! You go fear."),
		TEXT("I dey do this work since two years. I don carry everybody: pastor, yahoo boy, even one senator pikin."),
		TEXT("If rain fall now, price go up. No be me set am o, na the app."),
		TEXT("You wan make I on AC? AC na extra two hundred. I dey joke... small.") };
	const TCHAR* PassengerNames[] = { TEXT("Bisi"), TEXT("Ifeanyi"), TEXT("Halima"), TEXT("Dapo"), TEXT("Nkechi"), TEXT("Yusuf"), TEXT("Tolu"), TEXT("Ada") };
	const TCHAR* PassengerChat[] = {
		TEXT("Driver, abeg no rush. I never chop since morning."),
		TEXT("You sabi road? Because the last driver carry me go Ikorodu by mistake."),
		TEXT("Abeg small small for that bump. My phone screen don crack before."),
		TEXT("I go give you five star if we reach before my oga call me.") };
	const TCHAR* Hotline[] = {
		TEXT("Thank you for calling the Task Force hotline. Your report is number four thousand and twelve in the queue."),
		TEXT("We don hear you. Officer go come... when fuel dey the van."),
		TEXT("Caller, the person you dey report, na your landlord? We no dey settle rent matter."),
		TEXT("Your report has been forwarded to the Committee on Forwarding Reports.") };
}

ANHPhone::ANHPhone()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	const auto Add = [this](const TCHAR* Id, const TCHAR* Name, const TCHAR* Role, const FLinearColor& Color, bool bFriend, std::initializer_list<const TCHAR*> Hello)
	{
		FContact C;
		C.Id = Id;
		C.Name = Name;
		C.Role = Role;
		C.Color = Color;
		C.bFriend = bFriend;
		for (const TCHAR* Line : Hello)
		{
			C.Hello.Add(Line);
		}
		Contacts.Add(C);
	};
	Add(TEXT("baba"), TEXT("Baba Driver"), TEXT("Danfo owner, Oshodi"), FLinearColor(0.75f, 0.45f, 0.1f), false, { TEXT("Ehen, conductor! Wetin happen?"), TEXT("Bus dey for park. No waste my time o.") });
	Add(TEXT("mamangozi"), TEXT("Mama Ngozi"), TEXT("Provision shop, your street"), FLinearColor(0.7f, 0.15f, 0.4f), true, { TEXT("My pikin! You don chop today?"), TEXT("Shop dey, I dey. Wetin you need?") });
	Add(TEXT("iya"), TEXT("Iya Basira"), TEXT("Buka, Charity"), FLinearColor(0.1f, 0.5f, 0.3f), true, { TEXT("Ah, my customer! Amala dey hot now now."), TEXT("You wan come chop, abi na gist you carry come?") });
	Add(TEXT("amaka"), TEXT("Amaka Nwosu"), TEXT("Phone repairs, Yaba"), FLinearColor(0.2f, 0.45f, 0.85f), true, { TEXT("Lucky! Long time. How market?"), TEXT("I dey shop till evening. Talk to me.") });
	Add(TEXT("shina"), TEXT("Oga Shina"), TEXT("Mechanic, Shina Garage"), FLinearColor(0.35f, 0.35f, 0.4f), false, { TEXT("Shina Garage. Which motor spoil?"), TEXT("I fit come meet you for road, but e go cost you.") });
	Add(TEXT("kemi"), TEXT("Kemi Lawson-Bright"), TEXT("Lagos Island, business"), FLinearColor(0.55f, 0.3f, 0.7f), false, { TEXT("Kemi speaking. Make it quick, I am between meetings."), TEXT("When I have something for you, you will hear from me.") });
	Add(TEXT("taskforce"), TEXT("Task Force hotline"), TEXT("Report somebody"), FLinearColor(0.15f, 0.35f, 0.2f), false, {});
}

ANHPhone* ANHPhone::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	TActorIterator<ANHPhone> It(World);
	return World && It ? *It : nullptr;
}

const ANHPhone::FContact* ANHPhone::Find(FName Id) const
{
	return Contacts.FindByPredicate([Id](const FContact& C) { return C.Id == Id; });
}

ANHHUD* ANHPhone::Hud() const { return ANHHUD::Get(this); }

bool ANHPhone::Raining() const
{
	const ANHLightingRig* Rig = ANHLightingRig::Find(this);
	return Rig && Rig->Preset == ENHLightingPreset::NightRain;
}

bool ANHPhone::RushHour() const
{
	const UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const float Hour = Hustle ? Hustle->HourOfDay() : 12.f;
	return (Hour >= 7.f && Hour < 9.5f) || (Hour >= 16.f && Hour < 19.5f);
}

ANHVehicle* ANHPhone::PlayerCar() const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	return PC ? Cast<ANHVehicle>(PC->GetPawn()) : nullptr;
}

// ---------------------------------------------------------------------------------------------------- saved state
void ANHPhone::BeginPlay()
{
	Super::BeginPlay();
	State = Cast<UNHPhoneSave>(UGameplayStatics::LoadGameFromSlot(NHPhoneData::Slot, 0));
	if (!State)
	{
		State = NewObject<UNHPhoneSave>(this);
		for (const FContact& C : Contacts)
		{
			State->Friendship.Add(C.Id, C.bFriend ? 40 : 20);
		}
		Message(TEXT("baba"), TEXT("New conductor! Come Oshodi Motor Park make we talk. Bus no dey wait person."));
		Message(TEXT("mamangozi"), TEXT("My pikin, your garri dey shop. Come carry am before rat chop am."));
		Message(NHPhoneData::DropAm, TEXT("Welcome to DropAm! Okada, keke, car or luxury: we go drop you. Prices fit rise when rain fall. No vex."));
		Post(TEXT("Oshodi Updates"), TEXT("@OshodiUpdates"), TEXT("New face for Oshodi Motor Park this morning. Baba Driver don find another conductor."), false);
		Post(TEXT("DropAm Nigeria"), TEXT("@DropAmNG"), TEXT("Rain or shine, we dey drop you. (When rain fall, surge dey. We too dey hustle.)"), false);
	}
	Go(EPage::Home);
	NextIncoming = FMath::FRandRange(150.f, 260.f);
}

void ANHPhone::EndPlay(const EEndPlayReason::Type Reason)
{
	Save();
	Super::EndPlay(Reason);
}

void ANHPhone::Save()
{
	if (State)
	{
		UGameplayStatics::SaveGameToSlot(State, NHPhoneData::Slot, 0);
	}
}

int32 ANHPhone::FriendshipOf(FName Contact) const
{
	const int32* F = State ? State->Friendship.Find(Contact) : nullptr;
	return F ? *F : 0;
}

void ANHPhone::AddFriendship(FName Contact, int32 Amount)
{
	if (State)
	{
		int32& F = State->Friendship.FindOrAdd(Contact);
		F = FMath::Clamp(F + Amount, 0, 100);
		Save();
	}
}

void ANHPhone::Message(FName From, const FString& Text)
{
	if (!State)
	{
		return;
	}
	FNHPhoneMessage M;
	M.From = From;
	M.Text = Text;
	State->Chats.Add(M);
	if (State->Chats.Num() > 60)
	{
		State->Chats.RemoveAt(0);
	}
	if (HasActorBegunPlay() && GetGameTimeSinceCreation() > 1.f)
	{
		const FContact* C = Find(From);
		ANHHUD::Toast(this, FString::Printf(TEXT("Gist: %s sent a message"), C ? *C->Name : TEXT("DropAm")), 0);
	}
	Save();
}

void ANHPhone::Post(const FString& Author, const FString& Handle, const FString& Text, bool bAboutMe)
{
	if (!State)
	{
		return;
	}
	FNHPhonePost P;
	P.Author = Author;
	P.Handle = Handle;
	P.Text = Text;
	P.Likes = FMath::RandRange(40, 5000) * (bAboutMe ? 3 : 1);
	P.bMe = bAboutMe;
	State->Feed.Insert(P, 0);
	State->Feed.SetNum(FMath::Min(State->Feed.Num(), 30));
	++UnseenPosts;
	if (HasActorBegunPlay() && GetGameTimeSinceCreation() > 1.f)
	{
		ANHHUD::Toast(this, FString::Printf(TEXT("Yarns: %s posted%s"), *Handle, bAboutMe ? TEXT(" about you") : TEXT("")), 0);
	}
	Save();
}

int32 ANHPhone::Unseen() const
{
	int32 N = UnseenPosts + (State ? State->MissedCalls.Num() : 0);
	if (State)
	{
		for (const FNHPhoneMessage& M : State->Chats)
		{
			N += !M.bRead && !M.bMe ? 1 : 0;
		}
	}
	return N;
}

FString ANHPhone::Ringing() const
{
	const FContact* C = Call.bActive && Call.bIncoming && Call.RingLeft > 0.f ? Find(Call.Who) : nullptr;
	return C ? C->Name : FString();
}

// ------------------------------------------------------------------------------------------------------- pages
void ANHPhone::Go(EPage Page)
{
	Pages.Add(Page);
	Selected = 0;
	Scroll = 0;
}

void ANHPhone::Toggle()
{
	if (!bOpen && !Ringing().IsEmpty())
	{
		Select(); // opening the phone while it rings answers it
		return;
	}
	bOpen = !bOpen;
	if (bOpen && Pages.Num() == 0)
	{
		Go(EPage::Home);
	}
	if (bOpen)
	{
		Build();
	}
}

void ANHPhone::Back()
{
	if (!Ringing().IsEmpty())
	{
		// declined
		const FContact* C = Find(Call.Who);
		Call = FCall();
		AddFriendship(C->Id, -3);
		ANHHUD::Toast(this, FString::Printf(TEXT("You declined %s"), *C->Name), 0);
		return;
	}
	if (!bOpen)
	{
		return;
	}
	if (Pages.Num() > 1)
	{
		Pages.Pop();
		Selected = 0;
	}
	else
	{
		bOpen = false;
	}
}

void ANHPhone::Move(int32 Dir)
{
	if (!bOpen || Rows.Num() == 0)
	{
		return;
	}
	if (!Rows.ContainsByPredicate([](const FRow& R) { return R.bChoice; }))
	{
		Scroll = FMath::Clamp(Scroll + Dir, 0, FMath::Max(0, Rows.Num() - 3)); // nothing to choose: read down the page
		return;
	}
	// to the next row there is something to do on
	for (int32 Step = 0; Step < Rows.Num(); ++Step)
	{
		Selected = (Selected + Dir + Rows.Num()) % Rows.Num();
		if (Rows[Selected].bChoice)
		{
			break;
		}
	}
}

void ANHPhone::Change(int32 Dir)
{
	if (bOpen && Rows.IsValidIndex(Selected) && Rows[Selected].Change)
	{
		const TFunction<void(int32)> Fn = Rows[Selected].Change;
		Fn(Dir);
	}
}

void ANHPhone::Select()
{
	if (!Ringing().IsEmpty())
	{
		// answered: the friend says why they rang, then asks
		const FName Who = Call.Who;
		const FContact* C = Find(Who);
		Call.RingLeft = 0.f;
		Call.bIncoming = false;
		bOpen = true;
		PageContact = Who;
		Pages.Reset();
		Go(EPage::Home);
		Go(EPage::Contact);
		const bool bFood = Who == TEXT("iya") || Who == TEXT("mamangozi");
		Speak(Who, { bFood ? TEXT("I just cook fresh stew. Come chop before e finish!") : TEXT("Match dey for viewing centre this evening. You go come?"), TEXT("No tell me say you dey busy again o.") },
			[this]() { Go(EPage::HangOut); });
		AddFriendship(C->Id, 1);
		return;
	}
	if (Call.bActive && Call.Lines.IsValidIndex(Call.Line))
	{
		Call.LineLeft = 0.f; // Enter: next line
		return;
	}
	if (bOpen && Rows.IsValidIndex(Selected) && Rows[Selected].Do)
	{
		const TFunction<void()> Fn = Rows[Selected].Do; // a copy: the row may be rebuilt by what it does
		Fn();
	}
	else if (!bOpen && Ride.Stage == ERide::Trip)
	{
		// Enter with the phone away, mid-ride: skip to the end of the trip
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 1.2f, FLinearColor::Black, false, false);
		}
		Ride.Along = FMath::Max(Ride.Along, Ride.LineAt.Last() - 2500.f);
		Ride.At = FVector2D::ZeroVector; // snap, not ease
	}
}

ANHPhone::FRow& ANHPhone::Row(const FString& Text, const FString& Detail, TFunction<void()> Do)
{
	FRow& R = Rows.AddDefaulted_GetRef();
	R.Text = Text;
	R.Detail = Detail;
	R.Do = MoveTemp(Do);
	return R;
}

void ANHPhone::Info(const FString& Text, const FLinearColor& Color)
{
	FRow& R = Rows.AddDefaulted_GetRef();
	R.Text = Text;
	R.Color = Color;
	R.bChoice = false;
}

void ANHPhone::Build()
{
	using namespace NHPhoneData;
	Rows.Reset();
	Footer = TEXT("Up / Down   Enter: open   Backspace: back   P: put away");
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const UNHGameData* Data = UNHGameData::Get(this);
	const EPage Page = Pages.Num() ? Pages.Last() : EPage::Home;

	// on a call: only the line being spoken
	if (Call.bActive && !Call.bIncoming && Call.Lines.IsValidIndex(Call.Line))
	{
		const FContact* C = Find(Call.Who);
		Title = FString::Printf(TEXT("On call: %s"), C ? *C->Name : TEXT(""));
		Info(Call.Lines[Call.Line], FLinearColor(0.96f, 0.95f, 0.9f));
		Info(FString::Printf(TEXT("%d of %d"), Call.Line + 1, Call.Lines.Num()));
		Footer = TEXT("Enter: next line");
		return;
	}

	switch (Page)
	{
	case EPage::Home:
	{
		Title = Hustle ? Hustle->ClockText() : TEXT("Phone");
		int32 Unread = 0;
		for (const FNHPhoneMessage& M : State->Chats)
		{
			Unread += !M.bRead && !M.bMe ? 1 : 0;
		}
		const auto App = [this](const TCHAR* Name, const FString& Detail, const FLinearColor& Color, TFunction<void()> Do)
		{
			FRow& R = Row(Name, Detail, MoveTemp(Do));
			R.Badge = FString(Name).Left(1);
			R.BadgeColor = Color;
		};
		App(TEXT("Gist"), Unread ? FString::Printf(TEXT("%d unread"), Unread) : TEXT("Chats"), FLinearColor(0.1f, 0.55f, 0.35f), [this]() { Go(EPage::Gist); });
		App(TEXT("KoboPay"), Hustle ? UNHHustleSubsystem::Naira(Hustle->Cash) : FString(), FLinearColor(0.1f, 0.35f, 0.75f), [this]() { Go(EPage::Kobo); });
		App(TEXT("Yarns"), UnseenPosts ? FString::Printf(TEXT("%d new"), UnseenPosts) : TEXT("What Lagos is saying"), FLinearColor(0.1f, 0.6f, 0.7f), [this]() { UnseenPosts = 0; Go(EPage::Yarns); });
		App(TEXT("MapAm"), TEXT("Map and pin"), FLinearColor(0.75f, 0.5f, 0.1f), [this]() { bOpen = false; if (ANHHUD* H = Hud()) { H->ToggleMap(); } });
		App(TEXT("Contacts"), TEXT("Call somebody"), FLinearColor(0.5f, 0.3f, 0.65f), [this]() { Go(EPage::Contacts); });
		App(TEXT("DropAm"), Ride.Stage != ERide::None ? TEXT("Ride on the way") : TEXT("Order a ride"), FLinearColor(0.85f, 0.25f, 0.2f), [this]() { Go(EPage::DropAm); });
		App(TEXT("DropAm Driver"), Job.Stage != EJob::Offline ? TEXT("Online") : FString::Printf(TEXT("Rating %.1f"), State->DriverRating), FLinearColor(0.55f, 0.15f, 0.1f), [this]() { Go(EPage::DropAmDriver); });
		App(TEXT("Missed calls"), State->MissedCalls.Num() ? FString::Printf(TEXT("%d"), State->MissedCalls.Num()) : TEXT("None"), FLinearColor(0.4f, 0.4f, 0.4f), [this]() { Go(EPage::Missed); });
		App(TEXT("Camera"), TEXT("Take a photo"), FLinearColor(0.25f, 0.25f, 0.3f), [this]()
		{
			bOpen = false;
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				PC->ConsoleCommand(TEXT("shot")); // the view without the HUD
			}
			ANHHUD::Toast(this, TEXT("Photo saved in Saved/Screenshots"), 1);
		});
		break;
	}
	case EPage::Gist:
	{
		Title = TEXT("Gist");
		// one row a thread, the newest first
		TArray<FName> Seen;
		for (int32 I = State->Chats.Num() - 1; I >= 0; --I)
		{
			const FName From = State->Chats[I].From;
			if (Seen.Contains(From))
			{
				continue;
			}
			Seen.Add(From);
			const FContact* C = Find(From);
			const bool bUnread = State->Chats.ContainsByPredicate([From](const FNHPhoneMessage& M) { return M.From == From && !M.bRead && !M.bMe; });
			FRow& R = Row(C ? C->Name : TEXT("DropAm"), State->Chats[I].Text.Left(46) + (State->Chats[I].Text.Len() > 46 ? TEXT("...") : TEXT("")), [this, From]() { PageContact = From; Go(EPage::Thread); });
			R.Color = bUnread ? Yellow : FLinearColor(0.96f, 0.95f, 0.9f);
			R.Badge = R.Text.Left(1);
			R.BadgeColor = C ? C->Color : FLinearColor(0.85f, 0.25f, 0.2f);
		}
		if (Rows.Num() == 0)
		{
			Info(TEXT("No messages yet."));
		}
		break;
	}
	case EPage::Thread:
	{
		const FContact* C = Find(PageContact);
		Title = C ? C->Name : TEXT("DropAm");
		for (FNHPhoneMessage& M : State->Chats)
		{
			if (M.From == PageContact)
			{
				M.bRead = true;
				Info((M.bMe ? TEXT("You: ") : TEXT("")) + M.Text, M.bMe ? Good : FLinearColor(0.96f, 0.95f, 0.9f));
			}
		}
		if (C && C->Id != TEXT("taskforce"))
		{
			Row(TEXT("Call ") + C->Name, FString(), [this]() { CallContact(PageContact); });
			Selected = Rows.Num() - 1;
		}
		break;
	}
	case EPage::Kobo:
	{
		Title = TEXT("KoboPay");
		static const int32 Amounts[] = { 1000, 5000, 20000 };
		TArray<const FContact*> People;
		for (const FContact& C : Contacts)
		{
			if (C.Id != TEXT("taskforce") && C.Id != TEXT("shina"))
			{
				People.Add(&C);
			}
		}
		KoboTo = FMath::Clamp(KoboTo, 0, People.Num() - 1);
		Info(FString::Printf(TEXT("Balance  %s"), Hustle ? *UNHHustleSubsystem::Naira(Hustle->Cash) : TEXT("")), Yellow);
		Info(FString::Printf(TEXT("Integrity %d   Cred %d"), Hustle ? Hustle->Integrity : 0, Hustle ? Hustle->Cred : 0));
		Row(TEXT("Send to"), FString::Printf(TEXT("<  %s  >"), *People[KoboTo]->Name)).Change = [this, Count = People.Num()](int32 Dir) { KoboTo = (KoboTo + Dir + Count) % Count; };
		Row(TEXT("Amount"), FString::Printf(TEXT("<  %s  >"), *UNHHustleSubsystem::Naira(KoboAmount))).Change = [this](int32 Dir)
		{
			const int32 Now = KoboAmount == Amounts[1] ? 1 : KoboAmount == Amounts[2] ? 2 : 0;
			KoboAmount = Amounts[(Now + Dir + 3) % 3];
		};
		const FContact* To = People[KoboTo];
		Row(FString::Printf(TEXT("Send %s (+N50 fee)"), *UNHHustleSubsystem::Naira(KoboAmount)), FString(), [this, To, Hustle]()
		{
			if (!Hustle || Hustle->Cash < KoboAmount + 50)
			{
				KoboNote = TEXT("Insufficient funds. Hustle harder.");
			}
			else if (FMath::FRand() < 0.05f)
			{
				Hustle->Earn(-50, TEXT("Transfer fee"));
				KoboNote = TEXT("Transaction pending... Reversed. Network wahala. Fee still charged.");
			}
			else
			{
				Hustle->Earn(-(KoboAmount + 50), TEXT("Sent to ") + To->Name);
				KoboNote = FString::Printf(TEXT("Sent %s to %s."), *UNHHustleSubsystem::Naira(KoboAmount), *To->Name);
				Hustle->Integrity = FMath::Clamp(Hustle->Integrity + 1, -100, 100);
				AddFriendship(To->Id, KoboAmount >= 5000 ? 6 : 3);
				Message(To->Id, To->bFriend ? TEXT("I don see the alert. God go bless you!") : TEXT("Received. Thank you."));
			}
		}).Color = Yellow;
		if (!KoboNote.IsEmpty())
		{
			Info(KoboNote);
		}
		Info(TEXT("History"), Yellow);
		if (Hustle)
		{
			for (int32 I = 0; I < FMath::Min(6, Hustle->Ledger.Num()); ++I)
			{
				const FNHLedgerEntry& E = Hustle->Ledger[I];
				Info(FString::Printf(TEXT("%s%s  %s"), E.Amount > 0 ? TEXT("+") : TEXT(""), *UNHHustleSubsystem::Naira(E.Amount), *E.Why), E.Amount > 0 ? Good : Bad);
			}
		}
		break;
	}
	case EPage::Yarns:
		Title = TEXT("Yarns");
		for (const FNHPhonePost& P : State->Feed)
		{
			Info(FString::Printf(TEXT("%s %s"), *P.Author, *P.Handle), P.bMe ? Yellow : FLinearColor(0.4f, 0.8f, 0.9f));
			Info(P.Text, FLinearColor(0.96f, 0.95f, 0.9f));
			Info(FString::Printf(TEXT("%s likes"), *FText::AsNumber(P.Likes).ToString()));
		}
		if (State->Feed.Num() == 0)
		{
			Info(TEXT("Your feed is quiet. Do something worth talking about."));
		}
		break;
	case EPage::Contacts:
		Title = TEXT("Contacts");
		for (const FContact& C : Contacts)
		{
			const FName Id = C.Id;
			FRow& R = Row(C.Name, C.Role, [this, Id]() { PageContact = Id; Go(EPage::Contact); });
			R.Badge = C.Name.Left(1);
			R.BadgeColor = C.Color;
			R.Meter = C.Id == TEXT("taskforce") ? -1 : FriendshipOf(C.Id);
		}
		break;
	case EPage::Contact:
		if (const FContact* C = Find(PageContact))
		{
			Title = C->Name;
			ContactOptions(*C);
		}
		break;
	case EPage::HangOut:
		if (const FContact* C = Find(PageContact))
		{
			Title = TEXT("Hang out with ") + C->Name;
			Row(TEXT("Chop for buka"), TEXT("N1,500, 1 hour"), [this]() { HangOut(TEXT("chopped amala for buka"), 1500, 9, 1.f); });
			Row(TEXT("Watch ball for viewing centre"), TEXT("N500, 2 hours"), [this]() { HangOut(TEXT("watched the match"), 500, 7, 2.f); });
			Row(TEXT("Next time"), FString(), [this]() { AddFriendship(PageContact, -1); Back(); });
			Info(TEXT("Whot and Ludo are not in the game yet."));
		}
		break;
	case EPage::DropAm:
	{
		Title = TEXT("DropAm");
		if (!Data || !Data->bRealCity || !ANHTraffic::Get(this))
		{
			Info(TEXT("No DropAm drivers in this area."));
			break;
		}
		if (Ride.Stage != ERide::None)
		{
			const TCHAR* What = Ride.Stage == ERide::Finding ? TEXT("Finding you a driver...") : Ride.Stage == ERide::Coming ? TEXT("Your driver is on the way") : Ride.Stage == ERide::Waiting ? TEXT("Your driver is waiting for you")
				: Ride.Stage == ERide::Trip ? TEXT("On trip") : TEXT("Trip ended");
			Info(What, Yellow);
			if (Ride.Car.IsValid())
			{
				Info(FString::Printf(TEXT("%s, %s"), *Ride.Driver, *Ride.Car->DisplayName()));
				if (Ride.Stage == ERide::Coming)
				{
					const float Left = FMath::Max(0.f, Ride.LineAt.Last() - Ride.Along);
					Info(FString::Printf(TEXT("%d m away, about %d s"), FMath::RoundToInt(Left / 100.f), FMath::RoundToInt(Left / 1300.f)));
				}
			}
			Info(FString::Printf(TEXT("To %s   %s"), *Ride.DestName, Ride.bFriend ? TEXT("free: a friend's lift") : *UNHHustleSubsystem::Naira(Ride.Fare)));
			if (Ride.Stage == ERide::Trip)
			{
				Info(TEXT("Put the phone away and press Enter to skip the trip. F gets out here."));
			}
			else if (Ride.Stage != ERide::Leaving)
			{
				Row(TEXT("Cancel ride"), Ride.Stage == ERide::Finding || Ride.bFriend ? TEXT("free") : TEXT("N300 fee"), [this]() { CancelRide(TEXT("You cancelled")); }).Color = Bad;
			}
			break;
		}
		FVector2D Dest;
		FString DestName;
		const bool bDest = RideDestination(Dest, DestName);
		Row(TEXT("Where to"), FString::Printf(TEXT("<  %s  >"), bDest ? *DestName : TEXT("pin a place on MapAm"))).Change = [this, Data](int32 Dir)
		{
			RideDest = (RideDest + 1 + Dir + Data->Stops.Num() + 1) % (Data->Stops.Num() + 1) - 1;
		};
		float RoadLength = 0.f;
		const APawn* Pawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
		if (bDest && Pawn)
		{
			RoadLength = FVector2D::Distance(FVector2D(Pawn->GetActorLocation()), Dest) * 1.3f; // near enough for an estimate without routing every frame
		}
		for (int32 I = 0; I < 4; ++I)
		{
			float Surge = 1.f;
			const int32 Fare = FareFor(I, RoadLength, Surge);
			FRow& R = Row(RideTypes[I].Name, bDest ? FString::Printf(TEXT("about %s%s"), *UNHHustleSubsystem::Naira(Fare), Surge > 1.f ? *FString::Printf(TEXT("  x%.1f"), Surge) : TEXT("")) : FString(), [this, I]() { RideType = I; RequestRide(false); });
			R.Badge = FString(RideTypes[I].Name).Left(1);
			R.BadgeColor = FLinearColor(0.85f, 0.25f, 0.2f);
		}
		if (Raining() || RushHour())
		{
			Info(Raining() ? TEXT("Surge: rain dey fall, prices don rise.") : TEXT("Surge: rush hour, prices don rise."), Bad);
		}
		Info(FString::Printf(TEXT("Trips taken: %d"), State->RiderTrips));
		Footer = TEXT("Left / Right: where to   Enter: order   Backspace: back");
		break;
	}
	case EPage::DropAmDriver:
	{
		Title = TEXT("DropAm Driver");
		Info(FString::Printf(TEXT("Rating %.2f   Trips %d"), State->DriverRating, State->DriverTrips), Yellow);
		if (!Data || !Data->bRealCity)
		{
			Info(TEXT("No riders in this area."));
			break;
		}
		switch (Job.Stage)
		{
		case EJob::Offline:
			Row(TEXT("Go online"), PlayerCar() ? TEXT("take ride requests") : TEXT("get in your car first"), [this]()
			{
				const ANHVehicle* Car = PlayerCar();
				if (!Car || Car->GetSpec().bBike)
				{
					ANHHUD::Toast(this, Car ? TEXT("DropAm Driver is for cars and kekes") : TEXT("Get in a car first"), 2);
					return;
				}
				Job.Stage = EJob::Waiting;
				Job.Timer = FMath::FRandRange(6.f, 14.f);
				ANHHUD::Toast(this, TEXT("DropAm Driver: you are online"), 1);
			}).Color = Good;
			break;
		case EJob::Waiting:
			Info(TEXT("Online. Looking for riders near you..."));
			Row(TEXT("Go offline"), FString(), [this]() { EndJob(false); });
			break;
		case EJob::Offer:
			Info(FString::Printf(TEXT("%s wants a ride"), *Job.Passenger), Yellow);
			if (!Job.From.IsEmpty())
			{
				Info(TEXT("From ") + Job.From);
			}
			Info(FString::Printf(TEXT("To %s, about %s"), *Job.Where, *UNHHustleSubsystem::Naira(Job.Fare)));
			Row(TEXT("Accept"), FString::Printf(TEXT("%d s"), FMath::CeilToInt(Job.Timer)), [this]()
			{
				Job.Stage = EJob::ToPickup;
				if (ANHHUD* H = Hud())
				{
					H->SetPin(Job.Pickup, TEXT("DropAm pickup: ") + Job.Passenger);
				}
				bOpen = false;
			}).Color = Good;
			Row(TEXT("Decline"), FString(), [this]() { Job.Stage = EJob::Waiting; Job.Timer = FMath::FRandRange(8.f, 16.f); });
			break;
		case EJob::ToPickup:
			Info(FString::Printf(TEXT("Pick up %s (follow the pin)"), *Job.Passenger), Yellow);
			Row(TEXT("Cancel this trip"), TEXT("rating drops"), [this]() { EndJob(false); }).Color = Bad;
			break;
		case EJob::ToDropoff:
			Info(FString::Printf(TEXT("Take %s to %s"), *Job.Passenger, *Job.Where), Yellow);
			Info(FString::Printf(TEXT("Fare %s"), *UNHHustleSubsystem::Naira(Job.Fare)));
			break;
		}
		break;
	}
	case EPage::Missed:
		Title = TEXT("Missed calls");
		for (const FString& Who : State->MissedCalls)
		{
			Info(Who);
		}
		if (State->MissedCalls.Num() == 0)
		{
			Info(TEXT("Nobody called while you were away."));
		}
		else
		{
			Row(TEXT("Clear"), FString(), [this]() { State->MissedCalls.Reset(); Save(); });
		}
		break;
	}
	// keep the selection on something that can be chosen
	if (Rows.Num() > 0)
	{
		Selected = FMath::Clamp(Selected, 0, Rows.Num() - 1);
		for (int32 Step = 0; Step < Rows.Num() && !Rows[Selected].bChoice; ++Step)
		{
			Selected = (Selected + 1) % Rows.Num();
		}
	}
}

void ANHPhone::DebugOpen(const FString& What)
{
	bOpen = true;
	Pages.Reset();
	Go(EPage::Home);
	if (What == TEXT("contacts"))
	{
		Go(EPage::Contacts);
	}
	else if (What == TEXT("call"))
	{
		PageContact = TEXT("iya");
		Go(EPage::Contact);
		CallContact(PageContact);
	}
	else if (What == TEXT("dropam") || What == TEXT("ride"))
	{
		Go(EPage::DropAm);
		RideDest = 2;
		RideType = 2;
		RequestRide(false);
		Ride.bAutoBoard = What == TEXT("ride");
	}
	else if (What == TEXT("driver"))
	{
		Go(EPage::DropAmDriver);
	}
	Build();
}

// ------------------------------------------------------------------------------------------------------- calls
void ANHPhone::Speak(FName Who, const TArray<FString>& Lines, TFunction<void()> After)
{
	Call = FCall();
	Call.bActive = true;
	Call.Who = Who;
	Call.Lines = Lines;
	Call.After = MoveTemp(After);
	Call.LineLeft = 2.f + Lines[0].Len() * 0.05f;
}

void ANHPhone::CallContact(FName Who)
{
	const FContact* C = Find(Who);
	if (!C)
	{
		return;
	}
	PageContact = Who;
	if (Pages.Last() != EPage::Contact)
	{
		Go(EPage::Contact);
	}
	if (Who == TEXT("taskforce"))
	{
		Speak(Who, { NHPhoneData::Hotline[FMath::RandRange(0, 3)] }, [this]()
		{
			Post(TEXT("Eko Task Force (Official)"), TEXT("@EkoTaskForceOfficial"), TEXT("We received a report from a concerned citizen today. We are concerned too. Investigation is ongoing since."), false);
		});
	}
	else
	{
		Speak(Who, C->Hello);
	}
}

void ANHPhone::ContactOptions(const FContact& C)
{
	using namespace NHPhoneData;
	const FName Id = C.Id;
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	Info(C.Role);
	if (Id == TEXT("taskforce"))
	{
		Row(TEXT("Report somebody"), FString(), [this, Id]() { CallContact(Id); });
		Info(TEXT("Nothing has ever come of it."));
		return;
	}
	FRow& Meter = Rows.AddDefaulted_GetRef();
	Meter.Text = TEXT("Friendship");
	Meter.Color = Muted;
	Meter.bChoice = false;
	Meter.Meter = FriendshipOf(Id);
	Row(TEXT("Call"), TEXT("say hello"), [this, Id]() { CallContact(Id); AddFriendship(Id, 1); });
	Row(TEXT("Message on Gist"), FString(), [this, Id]() { PageContact = Id; Go(EPage::Thread); });
	if (C.bFriend)
	{
		Row(TEXT("Hang out"), FString(), [this, Id]()
		{
			const ANHGameDirector* Dir = ANHGameDirector::Get(this);
			if (Dir && Dir->DeadlineMinutesLeft >= 0.f)
			{
				Speak(Id, { TEXT("You dey work now. Finish your round first, then we go see.") });
				return;
			}
			Speak(Id, { TEXT("Ehen! Now you dey talk. Where we dey go?") }, [this]() { Go(EPage::HangOut); });
		});
		Row(TEXT("Ask for a lift"), FriendshipOf(Id) >= 30 ? TEXT("free") : TEXT("you are not close enough"), [this, Id]()
		{
			if (FriendshipOf(Id) < 30)
			{
				Speak(Id, { TEXT("Lift? You wey no dey even call me. Abeg enter DropAm.") });
				return;
			}
			if (Ride.Stage != ERide::None)
			{
				Speak(Id, { TEXT("You don already call ride. Enter that one.") });
				return;
			}
			Speak(Id, { TEXT("No wahala, I dey come carry you. Where you dey go, I go drop you."), TEXT("Tell me where for MapAm, then wait for me.") }, [this, Id]() { RequestRide(true, Id); });
		});
		Row(TEXT("Ask for backup"), FString(), [this, Id]()
		{
			// fights come with the weapons update: until then there is never one to help in
			Speak(Id, { TEXT("Backup? Who dey fight you? I no see anybody. Call me when wahala really start.") });
		});
		const int32 Loan = FMath::Min(10000, FriendshipOf(Id) * 100);
		Row(TEXT("Borrow money"), Loan >= 1000 ? UNHHustleSubsystem::Naira(Loan) : TEXT("not close enough"), [this, Id, Loan, Hustle]()
		{
			if (Loan < 1000 || !Hustle)
			{
				Speak(Id, { TEXT("Money? See this one. I never even see you finish.") });
				return;
			}
			Speak(Id, { TEXT("Ah. Okay o. I go send am, but no forget me when better come.") }, [this, Id, Loan, Hustle]()
			{
				Hustle->Earn(Loan, TEXT("Borrowed from ") + Find(Id)->Name);
				AddFriendship(Id, -12);
			});
		});
	}
	else if (Id == TEXT("shina"))
	{
		const ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
		ANHVehicle* Car = PC ? PC->GetLastVehicle() : nullptr;
		const int32 RepairFee = Car ? FMath::Max(2000, FMath::RoundToInt((Car->MaxHealth - Car->Health) * 60.f / 100.f) * 100 + 2000) : 0;
		Row(TEXT("Roadside repair"), Car ? UNHHustleSubsystem::Naira(RepairFee) : TEXT("you have no car"), [this, Id, Car, RepairFee, Hustle]()
		{
			if (!Car || !Hustle || Hustle->Cash < RepairFee)
			{
				Speak(Id, { Car ? TEXT("Your money no reach. Motor no dey repair itself.") : TEXT("Which motor? You never drive anything.") });
				return;
			}
			Speak(Id, { TEXT("Okay, my boy dey come with tools. Give am small time.") }, [this, Car, RepairFee, Hustle]()
			{
				Hustle->Earn(-RepairFee, TEXT("Shina Garage: roadside repair"));
				Car->Repair();
				ANHHUD::Toast(this, FString::Printf(TEXT("Your %s is repaired"), *Car->DisplayName()), 1);
			});
		});
		Row(TEXT("Tow my car to me"), Car ? TEXT("N5,000") : TEXT("you have no car"), [this, Id, Car, Hustle]()
		{
			const UNHGameData* Data = UNHGameData::Get(this);
			const APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
			FNHRoadSeg Seg;
			FVector2D Point;
			if (!Car || Car == Pawn || !Hustle || Hustle->Cash < 5000 || !Data || !Data->NearestRoad(FVector2D(Pawn->GetActorLocation()), Seg, Point))
			{
				Speak(Id, { !Car ? TEXT("Which motor? You never drive anything.") : Car == Pawn ? TEXT("You dey inside the motor. Wetin I wan tow?") : TEXT("Tow na five thousand. Call me when you get am.") });
				return;
			}
			Speak(Id, { TEXT("Tow truck dey come. E go drop the motor near you.") }, [this, Car, Hustle, Data, Seg, Point]()
			{
				const TArray<int32>& N = Data->RoadWays[Seg.Way].Nodes;
				const FVector2D Along = (Data->RoadNodes[N[Seg.Index + 1]] - Data->RoadNodes[N[Seg.Index]]).GetSafeNormal();
				const FVector2D At = Point + FVector2D(-Along.Y, Along.X) * (Data->HalfWidth(Data->RoadWays[Seg.Way]) - 140.f);
				Car->SetActorLocationAndRotation(FVector(At.X, At.Y, Car->GetActorLocation().Z + 100.f), FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X)), 0.f), false, nullptr, ETeleportType::TeleportPhysics);
				Hustle->Earn(-5000, TEXT("Shina Garage: tow"));
				ANHHUD::Toast(this, FString::Printf(TEXT("Your %s is at the kerb nearby"), *Car->DisplayName()), 1);
			});
		});
	}
	else if (Id == TEXT("baba"))
	{
		Row(TEXT("Where the park dey?"), TEXT("pins Oshodi Motor Park"), [this, Id]()
		{
			Speak(Id, { TEXT("Oshodi Motor Park! You no sabi Lagos? I don mark am for your map.") }, [this]()
			{
				const UNHGameData* Data = UNHGameData::Get(this);
				if (ANHHUD* H = Hud(); H && Data)
				{
					H->SetPin(Data->Park, TEXT("Oshodi Motor Park"));
				}
			});
		});
	}
}

void ANHPhone::HangOut(const FString& What, int32 Cost, int32 Gain, float Hours)
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const FContact* C = Find(PageContact);
	if (!Hustle || !C)
	{
		return;
	}
	if (Hustle->Cash < Cost)
	{
		Speak(C->Id, { TEXT("You no get money and you dey call person out? Next time.") });
		return;
	}
	// the outing itself is not played: a fade, the time passes, and you are closer for it
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 2.f, FLinearColor::Black, false, false);
	}
	Hustle->Earn(-Cost, FString::Printf(TEXT("Outing with %s"), *C->Name));
	Hustle->Minutes += Hours * 60.f;
	AddFriendship(C->Id, Gain);
	ANHHUD::Toast(this, FString::Printf(TEXT("You %s with %s. Friendship %d"), *What, *C->Name, FriendshipOf(C->Id)), 1);
	Message(C->Id, TEXT("Today sweet me. We go do am again."));
	Pages.Pop();
	Selected = 0;
}

// ------------------------------------------------------------------------------------------------ DropAm, rider
bool ANHPhone::RideDestination(FVector2D& Out, FString& Name) const
{
	const UNHGameData* Data = UNHGameData::Get(this);
	const ANHHUD* H = Hud();
	if (RideDest < 0)
	{
		if (H && H->bHasPin)
		{
			Out = H->Pin;
			Name = TEXT("your pin");
			return true;
		}
		return false;
	}
	int32 I = 0;
	for (const TPair<FName, FNHBusStop>& Stop : Data->Stops)
	{
		if (I++ == RideDest)
		{
			Out = Stop.Value.Kerb;
			Name = Stop.Value.Name;
			return true;
		}
	}
	return false;
}

int32 ANHPhone::FareFor(int32 Type, float RoadLength, float& OutSurge) const
{
	const NHPhoneData::FRideType& T = NHPhoneData::RideTypes[FMath::Clamp(Type, 0, 3)];
	OutSurge = (Raining() ? 1.6f : 1.f) * (RushHour() ? 1.4f : 1.f);
	return FMath::RoundToInt((T.Base + T.PerKm * RoadLength / 100000.f) * OutSurge / 50.f) * 50;
}

void ANHPhone::SetLine(const TArray<FVector2D>& Line)
{
	Ride.Line = Line;
	Ride.LineAt.Reset(Line.Num());
	float At = 0.f;
	for (int32 I = 0; I < Line.Num(); ++I)
	{
		At += I ? FVector2D::Distance(Line[I - 1], Line[I]) : 0.f;
		Ride.LineAt.Add(At);
	}
	Ride.Along = 0.f;
}

bool ANHPhone::SendCar(const FVector2D& To, float FromNear, float FromFar)
{
	const UNHGameData* Data = UNHGameData::Get(this);
	ANHTraffic* Traffic = ANHTraffic::Get(this);
	if (!Data || !Traffic)
	{
		return false;
	}
	for (int32 Try = 0; Try < 16; ++Try)
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const FVector2D From = To + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * FMath::FRandRange(FromNear, FromFar);
		TArray<FVector2D> Line;
		if (!Data->RoadRoute(From, To, Line))
		{
			continue;
		}
		float Length = 0.f;
		for (int32 I = 1; I < Line.Num(); ++I)
		{
			Length += FVector2D::Distance(Line[I - 1], Line[I]);
		}
		if (Length < FromNear * 0.6f || Length > FromFar * 2.2f)
		{
			continue; // on top of the player, or the long way round a one-way system
		}
		SetLine(Line);
		const FVector2D Dir = (Line[1] - Line[0]).GetSafeNormal();
		Ride.Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
		Ride.At = Line[0] + FVector2D(-Dir.Y, Dir.X) * 250.f;
		Ride.Car = Traffic->MakeForHire(Ride.Type, Ride.At, Ride.Yaw);
		return Ride.Car.IsValid();
	}
	return false;
}

void ANHPhone::RequestRide(bool bFromFriend, FName Friend)
{
	using namespace NHPhoneData;
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	const APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
	FVector2D Dest;
	FString DestName;
	if (Ride.Stage != ERide::None || !Pawn || !Hustle)
	{
		return;
	}
	if (Cast<ANHVehicle>(Pawn))
	{
		ANHHUD::Toast(this, TEXT("You are driving. Get out first."), 2);
		return;
	}
	if (!RideDestination(Dest, DestName))
	{
		ANHHUD::Toast(this, bFromFriend ? TEXT("Pin where you are going on MapAm, then ask again") : TEXT("DropAm: choose where to (pin a place on MapAm, or Left / Right for a stop)"), 2);
		return;
	}
	TArray<FVector2D> Line;
	const UNHGameData* Data = UNHGameData::Get(this);
	if (!Data->RoadRoute(FVector2D(Pawn->GetActorLocation()), Dest, Line))
	{
		ANHHUD::Toast(this, TEXT("DropAm: no road to there"), 2);
		return;
	}
	float Length = 0.f;
	for (int32 I = 1; I < Line.Num(); ++I)
	{
		Length += FVector2D::Distance(Line[I - 1], Line[I]);
	}
	float Surge = 1.f;
	Ride = FRide();
	Ride.bFriend = bFromFriend;
	Ride.Type = bFromFriend ? FName(TEXT("sedan")) : FName(RideTypes[RideType].Vehicle);
	Ride.Fare = bFromFriend ? 0 : FareFor(RideType, Length, Surge);
	Ride.TripLength = Length;
	Ride.Dest = Dest;
	Ride.DestName = DestName;
	Ride.Driver = bFromFriend ? Find(Friend)->Name : FString(DriverNames[FMath::RandRange(0, 7)]);
	if (Hustle->Cash < Ride.Fare)
	{
		ANHHUD::Toast(this, FString::Printf(TEXT("DropAm: the fare is %s and you have %s"), *UNHHustleSubsystem::Naira(Ride.Fare), *UNHHustleSubsystem::Naira(Hustle->Cash)), 2);
		Ride = FRide();
		return;
	}
	Ride.Stage = ERide::Finding;
	Ride.Timer = FMath::FRandRange(3.f, 6.f);
	ANHHUD::Toast(this, bFromFriend ? FString::Printf(TEXT("%s is coming to pick you up"), *Ride.Driver) : FString::Printf(TEXT("DropAm: finding a driver. %s to %s"), *UNHHustleSubsystem::Naira(Ride.Fare), *DestName), 0);
}

void ANHPhone::CancelRide(const FString& Why)
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	if (Ride.Stage == ERide::Coming || Ride.Stage == ERide::Waiting)
	{
		if (!Ride.bFriend && Hustle)
		{
			Hustle->Earn(-FMath::Min(300, Hustle->Cash), TEXT("DropAm cancellation fee"));
		}
		else if (Ride.bFriend)
		{
			if (const FContact* C = Contacts.FindByPredicate([this](const FContact& X) { return X.Name == Ride.Driver; }))
			{
				AddFriendship(C->Id, -6);
			}
		}
	}
	if (Ride.Car.IsValid())
	{
		Ride.Car->Destroy();
	}
	Ride = FRide();
	ANHHUD::Toast(this, TEXT("DropAm: ") + Why, 2);
}

bool ANHPhone::Drive(float DeltaSeconds, float Cruise)
{
	ANHVehicle* Car = Ride.Car.Get();
	if (!Car || Ride.Line.Num() < 2)
	{
		return true;
	}
	const float End = Ride.LineAt.Last();
	// slow for the last stretch, and for the player standing in the way
	float Want = FMath::Min(Cruise, FMath::Max(150.f, (End - Ride.Along) * 0.8f));
	if (Ride.Stage == ERide::Coming)
	{
		const APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
		const FVector2D To = Pawn ? FVector2D(Pawn->GetActorLocation()) - Ride.At : FVector2D(1e6f, 0.f);
		const float YawRad = FMath::DegreesToRadians(Ride.Yaw);
		const FVector2D Fwd(FMath::Cos(YawRad), FMath::Sin(YawRad));
		if ((To | Fwd) > 0.f && (To | Fwd) < 900.f && FMath::Abs(To | FVector2D(-Fwd.Y, Fwd.X)) < 220.f)
		{
			Want = 0.f;
		}
	}
	Ride.Speed = FMath::FInterpConstantTo(Ride.Speed, Want, DeltaSeconds, Want < Ride.Speed ? 1400.f : 500.f);
	Ride.Along = FMath::Min(End, Ride.Along + Ride.Speed * DeltaSeconds);
	int32 I = 1;
	while (I < Ride.Line.Num() - 1 && Ride.LineAt[I] < Ride.Along)
	{
		++I;
	}
	const FVector2D A = Ride.Line[I - 1], B = Ride.Line[I];
	const FVector2D Dir = (B - A).GetSafeNormal();
	const FVector2D Target = A + Dir * (Ride.Along - Ride.LineAt[I - 1]) + FVector2D(-Dir.Y, Dir.X) * 250.f; // keeping right
	const bool bSnap = Ride.At.IsZero();
	Ride.At = bSnap ? Target : FMath::Vector2DInterpTo(Ride.At, Target, DeltaSeconds, 5.f);
	const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	Ride.Yaw = bSnap ? TargetYaw : FMath::RInterpTo(FRotator(0.f, Ride.Yaw, 0.f), FRotator(0.f, TargetYaw, 0.f), DeltaSeconds, 3.5f).Yaw;
	Car->TrafficMove(Ride.At, Ride.Yaw, Ride.Speed, DeltaSeconds);
	return Ride.Along >= End - 1.f && Ride.Speed < 200.f;
}

void ANHPhone::Board()
{
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	ANHCharacter* Char = PC ? Cast<ANHCharacter>(PC->GetPawn()) : nullptr;
	ANHVehicle* Car = Ride.Car.Get();
	const UNHGameData* Data = UNHGameData::Get(this);
	TArray<FVector2D> Line;
	if (!Char || !Car || !Data->RoadRoute(FVector2D(Car->GetActorLocation()), Ride.Dest, Line))
	{
		CancelRide(TEXT("the driver cannot find a road to there"));
		return;
	}
	SetLine(Line);
	Ride.At = FVector2D(Car->GetActorLocation());
	Ride.TripLength = FMath::Max(1.f, Ride.LineAt.Last());
	// a passenger: the body goes along inside, unseen; the view is the car's
	Char->GetCharacterMovement()->DisableMovement();
	Char->SetActorHiddenInGame(true);
	Char->SetActorEnableCollision(false);
	Char->AttachToActor(Car, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	PC->SetViewTargetWithBlend(Car, 0.5f);
	PC->SetIgnoreMoveInput(true);
	Ride.Stage = ERide::Trip;
	Ride.ChatLeft = 4.f;
	bOpen = false;
	ANHHUD::Toast(this, TEXT("Enter: skip the trip     F: get out here"), 0);
}

void ANHPhone::EndTrip(bool bArrived)
{
	ANHPlayerController* PC = Cast<ANHPlayerController>(GetWorld()->GetFirstPlayerController());
	ANHCharacter* Char = PC ? PC->GetOnFootCharacter() : nullptr;
	ANHVehicle* Car = Ride.Car.Get();
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	if (Char && Car)
	{
		Char->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Char->SetActorLocationAndRotation(Car->ExitPoint(), FRotator(0.f, Car->GetActorRotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		Char->SetActorHiddenInGame(false);
		Char->SetActorEnableCollision(true);
		Char->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		PC->SetViewTargetWithBlend(Char, 0.5f);
		PC->ResetIgnoreMoveInput();
	}
	// what was driven is what is paid for
	const int32 Owed = Ride.bFriend ? 0 : FMath::RoundToInt(Ride.Fare * (bArrived ? 1.f : FMath::Clamp(Ride.Along / Ride.TripLength, 0.2f, 1.f)) / 50.f) * 50;
	if (Hustle && Owed > 0)
	{
		Hustle->Earn(-FMath::Min(Owed, Hustle->Cash), TEXT("DropAm ride to ") + Ride.DestName);
	}
	++State->RiderTrips;
	SubtitleSpeaker = Ride.Driver;
	Subtitle = bArrived ? (Ride.bFriend ? TEXT("We don reach. Greet your people for me.") : TEXT("We don reach. Abeg, five star o!")) : TEXT("Na here you dey drop? Okay o.");
	Call.LineLeft = 0.f;
	ANHHUD::Toast(this, Ride.bFriend ? FString::Printf(TEXT("%s dropped you at %s"), *Ride.Driver, *Ride.DestName) : FString::Printf(TEXT("DropAm: you paid %s"), *UNHHustleSubsystem::Naira(Owed)), 1);
	if (bArrived && Ride.DestName == TEXT("your pin"))
	{
		if (ANHHUD* H = Hud())
		{
			H->ClearPin();
		}
	}
	if (!Ride.bFriend && FMath::FRand() < 0.3f)
	{
		Post(TEXT("DropAm Chronicles"), TEXT("@DropAmChronicles"), FString::Printf(TEXT("My DropAm driver %s talk from pickup reach drop-off. I now know everything about fuel price."), *Ride.Driver), false);
	}
	Ride.Stage = ERide::Leaving;
	Ride.Timer = 6.f;
	Save();
}

bool ANHPhone::Interact()
{
	const APawn* Pawn = GetWorld()->GetFirstPlayerController()->GetPawn();
	if (Ride.Stage == ERide::Trip)
	{
		if (Ride.Speed > 500.f)
		{
			// he pulls up a little way on: the line is cut short there
			const float Cut = FMath::Min(Ride.LineAt.Last(), Ride.Along + 1800.f), Was = Ride.Along;
			TArray<FVector2D> Short;
			for (int32 I = 0; I < Ride.Line.Num(); ++I)
			{
				if (Ride.LineAt[I] < Cut)
				{
					Short.Add(Ride.Line[I]);
				}
				else
				{
					Short.Add(Ride.Line[I - 1] + (Ride.Line[I] - Ride.Line[I - 1]).GetSafeNormal() * (Cut - Ride.LineAt[I - 1]));
					break;
				}
			}
			if (Short.Num() >= 2)
			{
				SetLine(Short);
				Ride.Along = Was;
				Ride.bStopEarly = true;
				ANHHUD::Toast(this, TEXT("\"Driver, abeg stop here!\""), 0);
			}
			return true;
		}
		EndTrip(false);
		return true;
	}
	if (Ride.Stage == ERide::Waiting && Ride.Car.IsValid() && Pawn && FVector::Dist2D(Pawn->GetActorLocation(), Ride.Car->GetActorLocation()) < Ride.Car->EnterRadius() + 100.f)
	{
		Board();
		return true;
	}
	return false;
}

FString ANHPhone::InteractPrompt() const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (Ride.Stage == ERide::Trip)
	{
		return TEXT("Enter  Skip the trip      F  Get out here");
	}
	if (Ride.Stage == ERide::Waiting && Ride.Car.IsValid() && Pawn && FVector::Dist2D(Pawn->GetActorLocation(), Ride.Car->GetActorLocation()) < Ride.Car->EnterRadius() + 100.f)
	{
		return FString::Printf(TEXT("F  Get in: %s's %s"), *Ride.Driver, *Ride.Car->DisplayName());
	}
	return FString();
}

bool ANHPhone::RideMarker(FVector2D& Out, FString& Label) const
{
	if ((Ride.Stage == ERide::Coming || Ride.Stage == ERide::Waiting) && Ride.Car.IsValid())
	{
		Out = FVector2D(Ride.Car->GetActorLocation());
		Label = Ride.bFriend ? Ride.Driver : TEXT("DropAm");
		return true;
	}
	return false;
}

void ANHPhone::TickRide(float DeltaSeconds)
{
	using namespace NHPhoneData;
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	switch (Ride.Stage)
	{
	case ERide::Finding:
		Ride.Timer -= DeltaSeconds;
		if (Ride.Timer <= 0.f)
		{
			if (!Ride.bFriend && !Ride.bExcused && FMath::FRand() < 0.12f)
			{
				// the first driver drops the request
				Ride.bExcused = true;
				Ride.Timer = FMath::FRandRange(4.f, 7.f);
				Message(DropAm, FString::Printf(TEXT("%s cancelled your trip. We are finding you another driver."), *Ride.Driver));
				Ride.Driver = DriverNames[FMath::RandRange(0, 7)];
				break;
			}
			if (!SendCar(FVector2D(Pawn->GetActorLocation()), 12000.f, 26000.f))
			{
				CancelRide(TEXT("no drivers near you right now"));
				break;
			}
			Ride.Stage = ERide::Coming;
			Ride.Timer = 0.f;
			const float Roll = Ride.bFriend ? 1.f : FMath::FRand();
			if (Roll < 0.15f)
			{
				Ride.Timer = 8.f; // sits where he is a while first
				Message(DropAm, FString::Printf(TEXT("%s: I dey come, small go-slow for here. Two minutes."), *Ride.Driver));
			}
			else if (Roll < 0.27f && Ride.LineAt.Last() > 16000.f)
			{
				// stops well short and wants you to walk
				TArray<FVector2D> Short;
				for (int32 I = 0; I < Ride.Line.Num() && Ride.LineAt[I] < Ride.LineAt.Last() - 9000.f; ++I)
				{
					Short.Add(Ride.Line[I]);
				}
				if (Short.Num() >= 2)
				{
					SetLine(Short);
					Message(DropAm, FString::Printf(TEXT("%s: Abeg come meet me for junction, road no good to enter there. I don park."), *Ride.Driver));
				}
			}
			ANHHUD::Toast(this, FString::Printf(TEXT("%s is on the way in a %s (%d m)"), *Ride.Driver, *Ride.Car->DisplayName(), FMath::RoundToInt(Ride.LineAt.Last() / 100.f)), 1);
		}
		break;
	case ERide::Coming:
		if (Ride.Timer > 0.f)
		{
			Ride.Timer -= DeltaSeconds;
			break;
		}
		if (!Ride.Car.IsValid())
		{
			CancelRide(TEXT("your driver went offline"));
		}
		else if (Drive(DeltaSeconds, 1300.f))
		{
			Ride.Stage = ERide::Waiting;
			Ride.Timer = 90.f;
			const ANHStreets* Streets = ANHStreets::Get(this);
			const FString On = Streets ? Streets->StreetAt(Ride.At, 2500.f) : FString();
			ANHHUD::Toast(this, FString::Printf(TEXT("%s is here%s. Walk to the %s and press F"), *Ride.Driver, On.IsEmpty() ? TEXT("") : *(TEXT(" on ") + On), *Ride.Car->DisplayName()), 1);
		}
		break;
	case ERide::Waiting:
		if (Ride.bAutoBoard)
		{
			Board();
			break;
		}
		Ride.Timer -= DeltaSeconds;
		if (!Ride.Car.IsValid() || Ride.Timer <= 0.f)
		{
			CancelRide(Ride.bFriend ? TEXT("your friend got tired of waiting") : TEXT("your driver waited and left"));
		}
		break;
	case ERide::Trip:
		if (!Ride.Car.IsValid())
		{
			Ride = FRide();
			break;
		}
		Ride.ChatLeft -= DeltaSeconds;
		if (Ride.ChatLeft <= 0.f)
		{
			Ride.ChatLeft = FMath::FRandRange(8.f, 12.f);
			SubtitleSpeaker = Ride.Driver;
			Subtitle = Ride.bFriend ? TEXT("So how the hustle? You dey try, I dey see am.") : DriverChat[Ride.Chat++ % 8];
			Call.LineLeft = 0.f;
		}
		if (Drive(DeltaSeconds, 1500.f))
		{
			EndTrip(!Ride.bStopEarly);
		}
		break;
	case ERide::Leaving:
		Ride.Timer -= DeltaSeconds;
		if (Ride.Timer <= 0.f)
		{
			Subtitle.Reset();
			// gone once the player has walked off or looked away long enough
			if (!Ride.Car.IsValid() || Ride.Timer < -30.f || FVector::Dist2D(Pawn->GetActorLocation(), Ride.Car->GetActorLocation()) > 6000.f)
			{
				if (Ride.Car.IsValid() && !Ride.Car->GetController())
				{
					Ride.Car->Destroy();
				}
				Ride = FRide();
			}
		}
		break;
	default:
		break;
	}
}

// ----------------------------------------------------------------------------------------------- DropAm, driver
void ANHPhone::OfferJob()
{
	using namespace NHPhoneData;
	const UNHGameData* Data = UNHGameData::Get(this);
	const ANHVehicle* Car = PlayerCar();
	FNHRoadSeg Seg;
	if (!Data || !Car)
	{
		return;
	}
	for (int32 Try = 0; Try < 6; ++Try)
	{
		const FVector2D Here(Car->GetActorLocation());
		const float A = FMath::FRandRange(0.f, 2.f * PI), B = FMath::FRandRange(0.f, 2.f * PI);
		FVector2D Pickup, Dropoff;
		if (!Data->NearestRoad(Here + FVector2D(FMath::Cos(A), FMath::Sin(A)) * FMath::FRandRange(25000.f, 70000.f), Seg, Pickup))
		{
			continue;
		}
		if (!Data->NearestRoad(Pickup + FVector2D(FMath::Cos(B), FMath::Sin(B)) * FMath::FRandRange(100000.f, 250000.f), Seg, Dropoff))
		{
			continue;
		}
		TArray<FVector2D> Line;
		if (!Data->RoadRoute(Pickup, Dropoff, Line))
		{
			continue;
		}
		float Length = 0.f;
		for (int32 I = 1; I < Line.Num(); ++I)
		{
			Length += FVector2D::Distance(Line[I - 1], Line[I]);
		}
		float Surge = 1.f;
		Job.Pickup = Pickup;
		Job.Dropoff = Dropoff;
		Job.Fare = FareFor(2, Length, Surge);
		Job.Expected = Length / 1100.f + 25.f;
		Job.Passenger = PassengerNames[FMath::RandRange(0, 7)];
		// by street where the district has names: "Agege Motor Road, Oshodi"
		const ANHStreets* Streets = ANHStreets::Get(this);
		Job.Where = Streets ? Streets->PlaceName(Dropoff) : Data->DistrictAt(FVector(Dropoff, 0.f));
		Job.From = Streets ? Streets->PlaceName(Pickup) : FString();
		Job.Stage = EJob::Offer;
		Job.Timer = 20.f;
		ANHHUD::Toast(this, FString::Printf(TEXT("DropAm Driver: %s wants a ride%s to %s for %s. Open the phone (P)"), *Job.Passenger, Job.From.IsEmpty() ? TEXT("") : *(TEXT(" from ") + Job.From), *Job.Where, *UNHHustleSubsystem::Naira(Job.Fare)), 1);
		if (bOpen)
		{
			Pages.Reset();
			Go(EPage::Home);
			Go(EPage::DropAmDriver);
		}
		return;
	}
	Job.Timer = 10.f; // nobody this time
}

void ANHPhone::EndJob(bool bDone)
{
	UNHHustleSubsystem* Hustle = UNHHustleSubsystem::Get(this);
	if (Job.Body.IsValid())
	{
		Job.Body->Destroy();
	}
	if (bDone && Hustle)
	{
		// DropAm keeps a fifth; a quick, smooth trip earns a tip and the stars
		const float Late = (GetWorld()->GetTimeSeconds() - Job.Started) / Job.Expected;
		const float Stars = FMath::Clamp(5.f - FMath::Max(0.f, Late - 1.1f) * 3.f - Job.WorstBump, 1.f, 5.f);
		const int32 Tip = Stars >= 4.5f ? FMath::RoundToInt(Job.Fare * FMath::FRandRange(0.1f, 0.2f) / 50.f) * 50 : 0;
		const int32 Pay = FMath::RoundToInt(Job.Fare * 0.8f / 50.f) * 50;
		Hustle->Earn(Pay + Tip, FString::Printf(TEXT("DropAm trip: %s to %s"), *Job.Passenger, *Job.Where));
		State->DriverRating = (State->DriverRating * State->DriverTrips + Stars) / (State->DriverTrips + 1);
		++State->DriverTrips;
		ANHHUD::Toast(this, FString::Printf(TEXT("DropAm Driver: %s paid %s%s and gave you %.0f stars"), *Job.Passenger, *UNHHustleSubsystem::Naira(Pay), Tip ? *FString::Printf(TEXT(" + %s tip"), *UNHHustleSubsystem::Naira(Tip)) : TEXT(""), Stars), 1);
		if (ANHHUD* H = Hud())
		{
			H->ClearPin();
		}
		Job = FJob();
		Job.Stage = EJob::Waiting;
		Job.Timer = FMath::FRandRange(8.f, 18.f);
	}
	else
	{
		if (Job.Stage == EJob::ToPickup || Job.Stage == EJob::ToDropoff)
		{
			State->DriverRating = FMath::Max(1.f, State->DriverRating - 0.15f);
			ANHHUD::Toast(this, TEXT("DropAm Driver: trip cancelled. Your rating dropped."), 2);
			if (ANHHUD* H = Hud())
			{
				H->ClearPin();
			}
		}
		else
		{
			ANHHUD::Toast(this, TEXT("DropAm Driver: you are offline"), 0);
		}
		Job = FJob();
	}
	Save();
}

void ANHPhone::TickJob(float DeltaSeconds)
{
	using namespace NHPhoneData;
	if (Job.Stage == EJob::Offline)
	{
		return;
	}
	ANHVehicle* Car = PlayerCar();
	if (!Car)
	{
		EndJob(false); // out of the car: off the app
		return;
	}
	const FVector2D Here(Car->GetActorLocation());
	switch (Job.Stage)
	{
	case EJob::Waiting:
		Job.Timer -= DeltaSeconds;
		if (Job.Timer <= 0.f)
		{
			OfferJob();
		}
		break;
	case EJob::Offer:
		Job.Timer -= DeltaSeconds;
		if (Job.Timer <= 0.f)
		{
			State->DriverRating = FMath::Max(1.f, State->DriverRating - 0.03f);
			Job.Stage = EJob::Waiting;
			Job.Timer = FMath::FRandRange(8.f, 16.f);
			ANHHUD::Toast(this, TEXT("DropAm Driver: you missed that request"), 0);
		}
		break;
	case EJob::ToPickup:
		if (!Job.Body.IsValid() && FVector2D::Distance(Here, Job.Pickup) < 20000.f)
		{
			// the rider, waving at the roadside, made once you are near enough to see
			Job.Body = GetWorld()->SpawnActor<ANHPerson>(ANHPerson::StaticClass(), FVector(Job.Pickup.X, Job.Pickup.Y, 120.f), FRotator::ZeroRotator);
			if (Job.Body.IsValid())
			{
				Job.Body->Init(FMath::RandRange(1, 400), FLinearColor(0.85f, 0.25f, 0.2f));
				Job.Body->SetWaving(true);
			}
		}
		if (FVector2D::Distance(Here, Job.Pickup) < 1400.f && FMath::Abs(Car->Speed) < 150.f)
		{
			if (Job.Body.IsValid())
			{
				Job.Body->Destroy();
			}
			Job.Stage = EJob::ToDropoff;
			Job.Started = GetWorld()->GetTimeSeconds();
			Job.Timer = 5.f;
			if (ANHHUD* H = Hud())
			{
				H->SetPin(Job.Dropoff, FString::Printf(TEXT("drop %s at %s"), *Job.Passenger, *Job.Where));
			}
		}
		break;
	case EJob::ToDropoff:
	{
		// hard braking and crashes cost stars
		const float Jolt = FMath::Abs(Car->Speed - Job.LastSpeed) / FMath::Max(DeltaSeconds, 0.001f);
		Job.LastSpeed = Car->Speed;
		if (Jolt > 6000.f)
		{
			Job.WorstBump = FMath::Min(2.f, Job.WorstBump + 0.5f);
		}
		Job.Timer -= DeltaSeconds;
		if (Job.Timer <= 0.f)
		{
			Job.Timer = FMath::FRandRange(10.f, 15.f);
			SubtitleSpeaker = Job.Passenger;
			Subtitle = PassengerChat[FMath::RandRange(0, 3)];
			Call.LineLeft = 0.f;
		}
		if (FVector2D::Distance(Here, Job.Dropoff) < 1600.f && FMath::Abs(Car->Speed) < 150.f)
		{
			Subtitle.Reset();
			EndJob(true);
		}
		break;
	}
	default:
		break;
	}
}

// -------------------------------------------------------------------------------------------------------- tick
void ANHPhone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!State)
	{
		return;
	}
	// a call: each line stays up long enough to read, Enter moves on
	if (Call.bActive && !Call.bIncoming)
	{
		if (Call.Lines.IsValidIndex(Call.Line))
		{
			const FContact* C = Find(Call.Who);
			SubtitleSpeaker = C ? C->Name : FString();
			Subtitle = Call.Lines[Call.Line];
			Call.LineLeft -= DeltaSeconds;
			if (Call.LineLeft <= 0.f)
			{
				++Call.Line;
				Call.LineLeft = Call.Lines.IsValidIndex(Call.Line) ? 2.f + Call.Lines[Call.Line].Len() * 0.05f : 0.f;
			}
		}
		else
		{
			Subtitle.Reset();
			const TFunction<void()> After = MoveTemp(Call.After);
			Call = FCall();
			if (After)
			{
				After();
			}
		}
	}
	else if (Call.bActive && Call.bIncoming)
	{
		Call.RingLeft -= DeltaSeconds;
		if (Call.RingLeft <= 0.f)
		{
			const FContact* C = Find(Call.Who);
			State->MissedCalls.Insert(C->Name, 0);
			State->MissedCalls.SetNum(FMath::Min(State->MissedCalls.Num(), 8));
			Call = FCall();
			AddFriendship(C->Id, -4);
			Message(C->Id, TEXT("I call you, you no pick. Na so we dey do friend?"));
		}
	}
	else if (!Subtitle.IsEmpty() && Ride.Stage != ERide::Trip && Job.Stage != EJob::ToDropoff && Ride.Stage != ERide::Leaving)
	{
		Subtitle.Reset();
	}

	// now and then a friend rings: an invite, to take up or put off
	const ANHGameDirector* Dir = ANHGameDirector::Get(this);
	NextIncoming -= DeltaSeconds;
	if (NextIncoming <= 0.f)
	{
		NextIncoming = FMath::FRandRange(240.f, 420.f);
		if (!Call.bActive && Ride.Stage == ERide::None && Job.Stage == EJob::Offline && !(Dir && (Dir->IsBusy() || Dir->DeadlineMinutesLeft >= 0.f)))
		{
			TArray<FName> Friends;
			for (const FContact& C : Contacts)
			{
				if (C.bFriend)
				{
					Friends.Add(C.Id);
				}
			}
			Call = FCall();
			Call.bActive = true;
			Call.bIncoming = true;
			Call.Who = Friends[FMath::RandRange(0, Friends.Num() - 1)];
			Call.RingLeft = 14.f;
		}
	}
	TickRide(DeltaSeconds);
	TickJob(DeltaSeconds);
	if (bOpen)
	{
		Build();
	}
}
