// Copyright 2017-2018 Project Borealis

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "PBPlayerCharacter.generated.h"

class USoundCue;
class UPBMoveStepSound;
class UPBPlayerMovement;
class UCameraComponent;
class UCapsuleComponent;


USTRUCT()
struct FCameraSettings
{
	GENERATED_BODY()

		UPROPERTY(EditInstanceOnly, Category = "Camera", DisplayName = "Sensitivity (Yaw)", meta = (ClampMin = 0.0f, UIMax = 100.0f, ToolTip = "The sensitivity of the horizontal camera rotation (Yaw). Lower values = Slower camera rotation. Higher values = Faster camera rotation"))
		float SensitivityX = 50.0f;

	UPROPERTY(EditInstanceOnly, Category = "Camera", DisplayName = "Sensitivity (Pitch)", meta = (ClampMin = 0.0f, UIMax = 100.0f, ToolTip = "The sensitivity of the vertical camera rotation (Pitch). Lower values = Slower camera rotation. Higher values = Faster camera rotation"))
		float SensitivityY = 50.0f;

	UPROPERTY(EditInstanceOnly, Category = "Camera", meta = (ClampMin = "-360.0", ClampMax = 360.0f, ToolTip = "The minimum view pitch, in degrees. Some examples are 300.0, 340.0, -90.0, 270.0 or 0.0"))
		float MinPitch = -90.0f;

	UPROPERTY(EditInstanceOnly, Category = "Camera", meta = (ClampMin = "-360.0", ClampMax = 360.0f, ToolTip = "The maximum view pitch, in degrees. Some examples are 20.0, 45.0, 90.0 or 0.0"))
		float MaxPitch = 90.0f;
};


UCLASS(config = Game)
class PBCHARACTERMOVEMENT_API APBPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintGetter, Category = "Shooter | Character")
		USkeletalMeshComponent* GetFirstPersonArms() const { return FirstPersonArms; }
	// 1st Person Arms
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shooter | Character")
		class USkeletalMeshComponent* FirstPersonArms;

	// Ground friction while not crouching
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement, meta = (AllowPrivateAccess = "true"))
		float BaseGroundFriction;

	// Ground friction while crouching
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Movement, meta = (AllowPrivateAccess = "true"))
		float CrouchingGroundFriction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
		class UCameraComponent* CameraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
		class USpringArmComponent* SpringArmComponent;



	UFUNCTION()
	virtual void ClearJumpInput(float DeltaTime) override;
	virtual void StopJumping() override;
	virtual void OnJumped_Implementation() override;
	virtual bool CanJumpInternal_Implementation() const override;

	/* Triggered when player's movement mode has changed */
	void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PrevCustomMode) override;

	class APBPlayerController* GetPBController()
	{
		return PBController;
	}

	float GetLastJumpTime()
	{
		return LastJumpTime;
	}
protected:
	void BeginPlay() override;
	void Tick(float DeltaTime) override;
	void AddControllerYawInput(float Value) override;
	void AddControllerPitchInput(float Value) override;

	void MoveForward(float AxisValue);
	void MoveRight(float AxisValue);

private:

	APlayerController* PlayerController;

	// Footstep variables
	FVector LastFootstepLocation;
	FVector LastLocation;
	FFindFloorResult FloorResult;
	float TravelDistance = 0.0f;

	float OriginalCapsuleHalfHeight{};
	FVector OriginalCameraLocation; // Relative

	UPROPERTY(EditInstanceOnly, Category = "First Person Settings", meta = (ToolTip = "Enable this setting if you want to change the keys for specific action or axis mappings. Go to Project Settings -> Engine -> Input to update your inputs."))
		bool bUseCustomKeyMappings = false;


	class APBPlayerController* PBController;

	/** when we last jumped */
	float LastJumpTime;

	/** throttle jump boost when going up a ramp, so we don't spam it */
	float LastJumpBoostTime;

	/** maximum time it takes to jump */
	float MaxJumpTime;

	/** Automatic bunnyhopping */
	UPROPERTY(EditAnywhere, meta = (AllowPrivateAccess = "true"), Category = "PB Player|Gameplay")
	bool bAutoBunnyhop;

	/** Move step sounds by physical surface */
	UPROPERTY(EditDefaultsOnly, meta = (AllowPrivateAccess = "true"), Category = "PB Player|Sounds")
	TMap<TEnumAsByte<EPhysicalSurface>, TSubclassOf<UPBMoveStepSound>> MoveStepSounds;


	/** Pointer to player movement component */
	UPBPlayerMovement* MovementPtr;

	/** True if we're sprinting*/
	bool bIsSprinting;

	bool bWantsToWalk;

	virtual void ApplyDamageMomentum(float DamageTaken, FDamageEvent const& DamageEvent, APawn* PawnInstigator, AActor* DamageCauser) override;

protected:
	/* Called when the game starts or when spawned*/
	UPROPERTY(EditAnywhere, Category = "First Person Settings", meta = (ToolTip = "Adjust these camera settings to your liking"))
		FCameraSettings Camera;

public:
	APBPlayerCharacter(const FObjectInitializer& ObjectInitializer);

#pragma region Mutators
	UFUNCTION()
	bool IsSprinting() const
	{
		return bIsSprinting;
	}
	UFUNCTION()
	bool DoesWantToWalk() const
	{
		return bWantsToWalk;
	}
	UFUNCTION(Category = "PB Getters", BlueprintPure) FORCEINLINE bool GetAutoBunnyhop() const
	{
		return bAutoBunnyhop;
	};
	UFUNCTION(Category = "PB Setters", BlueprintCallable) void SetAutoBunnyhop(bool val)
	{
		bAutoBunnyhop = val;
	};
	UFUNCTION(Category = "PB Getters", BlueprintPure) FORCEINLINE UPBPlayerMovement* GetMovementPtr() const
	{
		return MovementPtr;
	};

#pragma endregion Mutators

	UFUNCTION()
	void ToggleNoClip();
};

