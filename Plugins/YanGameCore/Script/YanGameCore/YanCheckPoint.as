class AYanCheckPoint : AModularPlayerStart
{
    UPROPERTY()
    UBoxComponent Box;

    UFUNCTION(BlueprintOverride)
    void BeginPlay()
    {
        Box.OnComponentBeginOverlap.AddUFunction(this,n"OnBoxBeginOverlap");
    }

    UFUNCTION()
    private void OnBoxBeginOverlap(UPrimitiveComponent OverlappedComponent, AActor OtherActor, UPrimitiveComponent OtherComp, int OtherBodyIndex, bool bFromSweep, const FHitResult&in SweepResult)
    {
        
    }

}