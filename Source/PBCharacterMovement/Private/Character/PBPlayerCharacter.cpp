// Copyright 2017-2019 Project Borealis

#include "Character/PBPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "HAL/IConsoleManager.h"
#include "Character/PBPlayerMovement.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Components/InputComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"

static FName NAME_PBCharacterCollisionProfile_Capsule(TEXT("PBPawnCapsule"));
static FName NAME_PBCCharacterCollisionProfile_Mesh(TEXT("PBPawnMesh"));

static TAutoConsoleVariable<int32> CVarAutoBHop(TEXT("move.Pogo"), 0, TEXT("If holding spacebar should make the player jump whenever possible.\n"), ECVF_Default);

static TAutoConsoleVariable<int32> CVarBunnyhop(TEXT("move.Bunnyhopping"), 0, TEXT("Enable normal bunnyhopping.\n"), ECVF_Default);

// Sets default values
APBPlayerCharacter::APBPlayerCharacter(const FObjectInitializer& ObjectInitializer)
/* Override the movement class from the base class to our own to support multiple speeds (eg. sprinting) */
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPBPlayerMovement>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(FName("SpringArmComponent"));
	SpringArmComponent->TargetArmLength = 0.0f;
	SpringArmComponent->SetupAttachment(GetMesh());

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(FName("CameraComponent"));
	CameraComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	CameraComponent->bUsePawnControlRotation = true;
	CameraComponent->SetupAttachment(SpringArmComponent);


	FirstPersonArms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonArms"));
	FirstPersonArms->SetRelativeRotation(FRotator(0, -90, 0));
	CameraComponent->SetupAttachment(CameraComponent);

	// Other settings
	GetCapsuleComponent()->bReturnMaterialOnMove = true;

	BaseGroundFriction = 2.f;
	CrouchingGroundFriction = 100.f;

	// get pointer to movement component
	MovementPtr = Cast<UPBPlayerMovement>(ACharacter::GetMovementComponent());

	// Don't rotate when the controller rotates. Let the controller only affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = false; // Character moves in the direction of input...
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f); // ...at this rotation rate

	/*Net Settings*/
//NetUpdateFrequency = 128.0f;
//MinNetUpdateFrequency = 32.0f;
}

void APBPlayerCharacter::BeginPlay()
{
	// Call the base class
	Super::BeginPlay();
	// Max jump time to get to the top of the arc
	MaxJumpTime = -4.0f * GetCharacterMovement()->JumpZVelocity / (3.0f * GetCharacterMovement()->GetGravityZ());

	APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (CameraManager)
	{
		CameraManager->ViewPitchMin = Camera.MinPitch;
		CameraManager->ViewPitchMax = Camera.MaxPitch;
	}

	// Initialization
	OriginalCameraLocation = CameraComponent->GetRelativeLocation();
	OriginalCapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	// Footstep setup
	LastLocation = GetActorLocation();
	LastFootstepLocation = GetActorLocation();
	TravelDistance = 0;


}

void APBPlayerCharacter::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
}
void APBPlayerCharacter::Move(FVector Direction, float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}
void APBPlayerCharacter::MoveForward(const float AxisValue)
{
	if (Controller)
	{
		FRotator ForwardRotation = Controller->GetControlRotation();

		// Limit pitch rotation
		if (GetCharacterMovement()->IsMovingOnGround() || GetCharacterMovement()->IsFalling())
			ForwardRotation.Pitch = 0.0f;

		// Find out which way is forward
		const FVector Direction = FRotationMatrix(ForwardRotation).GetScaledAxis(EAxis::X);

		// Apply movement in the calculated direction
		AddMovementInput(Direction, AxisValue);

		}
}

void APBPlayerCharacter::MoveRight(const float AxisValue)
{
	if (Controller)
	{
		// Find out which way is right
		const FRotator RightRotation = Controller->GetControlRotation();
		const FVector Direction = FRotationMatrix(RightRotation).GetScaledAxis(EAxis::Y);

		// Apply movement in the calculated direction
		AddMovementInput(Direction, AxisValue);
	}
}

void APBPlayerCharacter::AddControllerYawInput(const float Value)
{
	return Super::AddControllerYawInput(Value * Camera.SensitivityX * GetWorld()->GetDeltaSeconds());
}

void APBPlayerCharacter::AddControllerPitchInput(const float Value)
{
	Super::AddControllerPitchInput(Value * Camera.SensitivityY * GetWorld()->GetDeltaSeconds());
}































void APBPlayerCharacter::ClearJumpInput(float DeltaTime)
{
	// Don't clear jump input right away if we're auto hopping or noclipping (holding to go up)
	if (CVarAutoBHop->GetInt() != 0 || bAutoBunnyhop || GetCharacterMovement()->bCheatFlying)
	{
		return;
	}
	Super::ClearJumpInput(DeltaTime);
}

void APBPlayerCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PrevCustomMode)
{
	if (!bPressedJump)
	{
		ResetJumpState();
	}

	if (GetCharacterMovement()->IsFalling())
	{
		// Record jump force start time for proxies. Allows us to expire the jump even if not continually ticking down a timer.
		if (bProxyIsJumpForceApplied)
		{
			ProxyJumpForceStartedTime = GetWorld()->GetTimeSeconds();
		}
	}
	else
	{
		JumpCurrentCount = 0;
		JumpKeyHoldTime = 0.0f;
		JumpForceTimeRemaining = 0.0f;
		bWasJumping = false;
	}

	K2_OnMovementModeChanged(PrevMovementMode, GetCharacterMovement()->MovementMode, PrevCustomMode, GetCharacterMovement()->CustomMovementMode);
	MovementModeChangedDelegate.Broadcast(this, PrevMovementMode, PrevCustomMode);
}

void APBPlayerCharacter::StopJumping()
{
	Super::StopJumping();
	if (GetCharacterMovement()->bCheatFlying)
	{
		Cast<UPBPlayerMovement>(GetMovementComponent())->NoClipVerticalMoveMode = 0;
	}
}

void APBPlayerCharacter::OnJumped_Implementation()
{
	LastJumpTime = GetWorld()->GetTimeSeconds();
	if (GetCharacterMovement()->bCheatFlying)
	{
		MovementPtr->NoClipVerticalMoveMode = 1;
	}
	else if (GetWorld()->GetTimeSeconds() >= LastJumpBoostTime + MaxJumpTime)
	{
		LastJumpBoostTime = GetWorld()->GetTimeSeconds();
		// Boost forward speed on jump
		FVector Facing = GetActorForwardVector();
		// Give it its backward/forward direction
		float ForwardSpeed;
		FVector Input = MovementPtr->GetLastInputVector().GetClampedToMaxSize2D(1.0f) * MovementPtr->GetMaxAcceleration();
		ForwardSpeed = Input | Facing;
		// Adjust how much the boost is
		float SpeedBoostPerc = (bIsSprinting || bIsCrouched) ? 0.1f : 0.5f;
		// How much we are boosting by
		float SpeedAddition = FMath::Abs(ForwardSpeed * SpeedBoostPerc);
		// We can only boost up to this much
		float MaxBoostedSpeed = GetCharacterMovement()->GetMaxSpeed() + GetCharacterMovement()->GetMaxSpeed() * SpeedBoostPerc;
		// Calculate new speed
		float NewSpeed = SpeedAddition + GetMovementComponent()->Velocity.Size2D();
		float SpeedAdditionNoClamp = SpeedAddition;

		// Scale the boost down if we are going over
		if (NewSpeed > MaxBoostedSpeed)
		{
			SpeedAddition -= NewSpeed - MaxBoostedSpeed;
		}

		if (ForwardSpeed < -MovementPtr->GetMaxAcceleration() * FMath::Sin(0.6981f))
		{
			// Boost backwards if we're going backwards
			SpeedAddition *= -1.0f;
			SpeedAdditionNoClamp *= -1.0f;
		}

		// Boost our velocity
		FVector JumpBoostedVel = GetMovementComponent()->Velocity + Facing * SpeedAddition;
		float JumpBoostedSizeSq = JumpBoostedVel.SizeSquared2D();
		if (CVarBunnyhop->GetInt() != 0)
		{
			FVector JumpBoostedUnclampVel = GetMovementComponent()->Velocity + Facing * SpeedAdditionNoClamp;
			float JumpBoostedUnclampSizeSq = JumpBoostedUnclampVel.SizeSquared2D();
			if (JumpBoostedUnclampSizeSq > JumpBoostedSizeSq)
			{
				JumpBoostedVel = JumpBoostedUnclampVel;
				JumpBoostedSizeSq = JumpBoostedUnclampSizeSq;
			}
		}
		if (GetMovementComponent()->Velocity.SizeSquared2D() < JumpBoostedSizeSq)
		{
			GetMovementComponent()->Velocity = JumpBoostedVel;
		}
	}
}

void APBPlayerCharacter::ToggleNoClip()
{
	MovementPtr->ToggleNoClip();
}

bool APBPlayerCharacter::CanJumpInternal_Implementation() const
{
	bool bCanJump = GetCharacterMovement() && GetCharacterMovement()->IsJumpAllowed();

	if (bCanJump)
	{
		// Ensure JumpHoldTime and JumpCount are valid.
		if (!bWasJumping || GetJumpMaxHoldTime() <= 0.0f)
		{
			if (JumpCurrentCount == 0 && GetCharacterMovement()->IsFalling())
			{
				bCanJump = JumpCurrentCount + 1 < JumpMaxCount;
			}
			else
			{
				bCanJump = JumpCurrentCount < JumpMaxCount;
			}
		}
		else
		{
			// Only consider JumpKeyHoldTime as long as:
			// A) We are on the ground
			// B) The jump limit hasn't been met OR
			// C) The jump limit has been met AND we were already jumping
			const bool bJumpKeyHeld = (bPressedJump && JumpKeyHoldTime < GetJumpMaxHoldTime());
			bCanJump = bJumpKeyHeld &&
				(GetCharacterMovement()->IsMovingOnGround() || (JumpCurrentCount < JumpMaxCount) || (bWasJumping && JumpCurrentCount == JumpMaxCount));
		}
		if (GetCharacterMovement()->IsMovingOnGround())
		{
			float FloorZ = FVector(0.0f, 0.0f, 1.0f) | GetCharacterMovement()->CurrentFloor.HitResult.ImpactNormal;
			float WalkableFloor = GetCharacterMovement()->GetWalkableFloorZ();
			bCanJump &= (FloorZ >= WalkableFloor || FMath::IsNearlyEqual(FloorZ, WalkableFloor));
		}
	}

	return bCanJump;
}

void APBPlayerCharacter::ApplyDamageMomentum(float DamageTaken, FDamageEvent const& DamageEvent, APawn* PawnInstigator, AActor* DamageCauser)
{
	UDamageType const* const DmgTypeCDO = DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>();
	if (GetCharacterMovement())
	{
		FHitResult HitInfo;
		FVector ImpulseDir;
		DamageEvent.GetBestHitInfo(this, DamageCauser, HitInfo, ImpulseDir);

		FVector Impulse = ImpulseDir;
		float SizeFactor = (60.96f * 60.96f * 137.16f) /
						   (FMath::Square(GetCapsuleComponent()->GetScaledCapsuleRadius() * 2.0f) * GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0f);
		Impulse *= DamageTaken * SizeFactor * 5.0f * 3.0f / 4.0f;
		bool const bMassIndependentImpulse = !DmgTypeCDO->bScaleMomentumByMass;
		FVector MassScaledImpulse = Impulse;
		if (!bMassIndependentImpulse && GetCharacterMovement()->Mass > SMALL_NUMBER)
		{
			MassScaledImpulse = MassScaledImpulse / GetCharacterMovement()->Mass;
		}
		if (MassScaledImpulse.Z > 1238.25f)
		{
			Impulse.Z = 1238.25f;
		}
		if (MassScaledImpulse.SizeSquared2D() > 1714.5f * 1714.5f)
		{
			// Impulse = Impulse.GetClampedToMaxSize2D(1714.5f);
		}

		GetCharacterMovement()->AddImpulse(Impulse, bMassIndependentImpulse);
	}
}