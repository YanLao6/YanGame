class ACuttableActor : AYanCuttableActor
{
	UPROPERTY(DefaultComponent)
	UStaticMeshComponent CutMesh;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		InitializeFromStaticMesh(CutMesh.StaticMesh);

		CutMesh.SetHiddenInGame(false);
	}
}