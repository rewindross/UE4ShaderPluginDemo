/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2015-2020 Fredrik Lindh
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
******************************************************************************/

#include "ShaderPluginDemoCharacter.h"

#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Runtime/Engine/Classes/Materials/MaterialInstanceDynamic.h"

// TODO:
// Shader lifetime. We probably need to hook up the render callback in the plugin module for more stability.
// Getting the output into a rendertarget with new pipeline
// Super annoying with the shaders not being recompiled

AShaderPluginDemoCharacter::AShaderPluginDemoCharacter() 
{
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeRotation(FRotator(1.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-0.5f, -4.4f, -155.7f));

	FP_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FP_Gun"));
	FP_Gun->SetOnlyOwnerSee(true);
	FP_Gun->bCastDynamicShadow = false;
	FP_Gun->CastShadow = false;
	FP_Gun->SetupAttachment(RootComponent);

	FP_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	FP_MuzzleLocation->SetupAttachment(FP_Gun);
	FP_MuzzleLocation->SetRelativeLocation(FVector(0.2f, 48.4f, -10.6f));

	GunOffset = FVector(100.0f, 0.0f, 10.0f);

	// If this is set to true, we will draw every frame, and we should call UpdateParameters() when we need to.
	// If this is false, we should call Draw() every time we want to draw.
	bRenderEveryFrame = true;

	ShaderUsageExample = new FShaderUsageExample(bRenderEveryFrame, FIntPoint(1024, 1024));
	EndColorBuildup = 0;
	EndColorBuildupDirection = 1;
	PixelShaderTopLeftColor = FColor::Green;
	ComputeShaderSimulationSpeed = 1.0;
	ComputeShaderBlend = 0.5f;
	ComputeShaderBlendScalar = 0;
	TotalElapsedTime = 0;
	bSaveComputeShaderOutput = false;
	bSavePixelShaderOutput = false;
}

void AShaderPluginDemoCharacter::BeginPlay() 
{
	Super::BeginPlay();
	FP_Gun->AttachToComponent(Mesh1P, FAttachmentTransformRules::SnapToTargetIncludingScale, TEXT("GripPoint"));
}

void AShaderPluginDemoCharacter::BeginDestroy() 
{
	Super::BeginDestroy();

// 	if (ShaderUsageExample)
// 	{
// 		delete ShaderUsageExample;
// 	}
}

void AShaderPluginDemoCharacter::SavePixelShaderOutput() 
{
	bSavePixelShaderOutput = true;
}
void AShaderPluginDemoCharacter::SaveComputeShaderOutput() 
{
	bSaveComputeShaderOutput = true;
}

void AShaderPluginDemoCharacter::ModifyComputeShaderBlend(float NewScalar) 
{
	ComputeShaderBlendScalar = NewScalar;
}

void AShaderPluginDemoCharacter::Tick(float DeltaSeconds) 
{
	Super::Tick(DeltaSeconds);

	TotalElapsedTime += DeltaSeconds;

	if (ShaderUsageExample)
	{
		EndColorBuildup = FMath::Clamp(EndColorBuildup + DeltaSeconds * EndColorBuildupDirection, 0.0f, 1.0f);
		if (EndColorBuildup >= 1.0 || EndColorBuildup <= 0) 
		{
			EndColorBuildupDirection *= -1;
		}
		
		ComputeShaderBlend = FMath::Clamp(ComputeShaderBlend + ComputeShaderBlendScalar * DeltaSeconds, 0.0f, 1.0f);

		FShaderUsageExampleParameters DrawParameters(GetWorld()->Scene->GetFeatureLevel(), RenderTarget, DeltaSeconds, TotalElapsedTime);
		DrawParameters.SimulationSpeed = ComputeShaderSimulationSpeed;
		DrawParameters.ComputeShaderBlend = ComputeShaderBlend;
		DrawParameters.bSaveComputeShaderOutput = bSaveComputeShaderOutput;
		DrawParameters.bSavePixelShaderOutput = bSavePixelShaderOutput;
		DrawParameters.StartColor = PixelShaderTopLeftColor;
		DrawParameters.EndColor = FColor(EndColorBuildup * 255, 0, 0, 255);

		if (bRenderEveryFrame)
		{
			// If doing this for realsies, you should avoid doing this every frame unless you have to of course.
			ShaderUsageExample->UpdateParameters(DrawParameters);
		}
		else
		{
			ShaderUsageExample->Draw(DrawParameters);
		}
	}

	bSavePixelShaderOutput = false;
	bSaveComputeShaderOutput = false;
}

void AShaderPluginDemoCharacter::OnFire()
{
	// Try to set a texture to the object we hit!
	FHitResult HitResult;
	FVector StartLocation = FirstPersonCameraComponent->GetComponentLocation();
	FRotator Direction = FirstPersonCameraComponent->GetComponentRotation();
	FVector EndLocation = StartLocation + Direction.Vector() * 10000;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, QueryParams))
	{
		TArray<UStaticMeshComponent*> StaticMeshComponents = TArray<UStaticMeshComponent*>();
		AActor* HitActor = HitResult.GetActor();

		if (nullptr != HitActor)
		{
			HitActor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);

			for (int32 i = 0; i < StaticMeshComponents.Num(); i++) 
			{
				UStaticMeshComponent* CurrentStaticMeshPtr = StaticMeshComponents[i];
				CurrentStaticMeshPtr->SetMaterial(0, MaterialToApplyToClickedObject);
				UMaterialInstanceDynamic* MID =	CurrentStaticMeshPtr->CreateAndSetMaterialInstanceDynamic(0);
				MID->SetTextureParameterValue("InputTexture", (UTexture*)RenderTarget);
			}
		}
	}

	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
	}

	UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
	if (AnimInstance && FireAnimation)
	{
		AnimInstance->Montage_Play(FireAnimation, 1.f);
	}
}

void AShaderPluginDemoCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AShaderPluginDemoCharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AShaderPluginDemoCharacter::StopJumping);
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &AShaderPluginDemoCharacter::OnFire);

	PlayerInputComponent->BindAxis("MoveForward", this, &AShaderPluginDemoCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AShaderPluginDemoCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AShaderPluginDemoCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AShaderPluginDemoCharacter::LookUpAtRate);
	
	PlayerInputComponent->BindAction("SavePixelShaderOutput", IE_Pressed, this, &AShaderPluginDemoCharacter::SavePixelShaderOutput);
	PlayerInputComponent->BindAction("SaveComputeShaderOutput", IE_Pressed, this, &AShaderPluginDemoCharacter::SaveComputeShaderOutput);
	PlayerInputComponent->BindAxis("ComputeShaderBlend", this, &AShaderPluginDemoCharacter::ModifyComputeShaderBlend);
}

void AShaderPluginDemoCharacter::MoveForward(float Value) 
{
	if (Value != 0.0f) 
	{
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AShaderPluginDemoCharacter::MoveRight(float Value) 
{
	if (Value != 0.0f) 
	{
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void AShaderPluginDemoCharacter::TurnAtRate(float Rate) 
{
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AShaderPluginDemoCharacter::LookUpAtRate(float Rate) 
{
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}
