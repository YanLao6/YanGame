// Copyright (c) Yevhenii Selivanov

#pragma once

#include "Subsystems/WorldSubsystem.h"

// Pool Manager
#include "Data/PoolContainer.h"
#include "Data/SpawnRequestPriority.h"

// UE
#include "GameFeatureStateChangeObserver.h"

#include "PoolManagerSubsystem.generated.h"

enum class EPoolObjectState : uint8;

typedef TFunction<void(const struct FPoolObjectData&)> FOnSpawnCallback;
typedef TFunction<void(const TArray<struct FPoolObjectData>&)> FOnSpawnAllCallback;

/**
 * Pool Manager 用于复用高频出现的对象，避免每次都重新创建与销毁。
 *
 * 要解决的问题：
 *     在 Unreal Engine 中创建和销毁投射物、爆炸特效之类的对象开销较大。
 *     高频反复创建销毁会导致卡顿与掉帧。
 *
 * Pool Manager 的作用：
 *     维护一个对象池，对象用完后归还而不是销毁，下次直接复用，从而让运行更平稳。
 *
 * 工作方式：
 *     对象从池中取出使用，用完归还，归还后进入 'inactive'（非激活）状态。
 *     对于 Actor，归还时会被移出关卡可视范围、隐藏，不再参与交互，也不再消耗运行时资源。
 *
 * 代码架构：
 *		 - Pool Manager 是自动创建的 Subsystem，其子类同样会被自动创建。
 *		 - 它只存储和管理数据（各个 Pool 及其中的对象）。
 *		 - 它不负责对象的具体行为逻辑，只负责与数据相关的基础池化逻辑。
 *		 - 对象的具体行为（创建、销毁、可见性等）由 Pool Factory 负责。
 *		 - 需要自定义逻辑时，优先重写 Pool Factory，而不是 Pool Manager。
 *
 * 可选特性：
 *     - Pool Manager 在进入游戏之前即可用，因此可以在关卡中预构建 Actor。
 *     - 可以继承本类并重写其函数，实现自定义的 Pool Manager。
 */
UCLASS(BlueprintType, Blueprintable)
class POOLMANAGER_API UPoolManagerSubsystem : public UWorldSubsystem
    , public IGameFeatureStateChangeObserver
{
	GENERATED_BODY()

	/*********************************************************************************************
	 * 静态获取接口
	 ********************************************************************************************* */
public:
	/** 返回 Pool Manager，内部使用 checked 校验，获取失败会崩溃。
	 * 适合在代码中使用，多数情况下无需传参即可拿到默认 Pool Manager。
	 * 例如：
	 * - UPoolManagerSubsystem::Get().TakeFromPool...
	 * - UPoolManagerSubsystem::Get<UCustomCodePoolManager>().TakeFromPool...
	 * - UPoolManagerSubsystem::Get(CustomBlueprintPoolManager).TakeFromPool...
	 * @tparam T 可选，若在代码中实现了自己的 Pool Manager，传入你的子类。
	 * @param OptionalClass 可选，若在蓝图中实现了自己的 Pool Manager，指定其类。
	 * @param OptionalWorldContext 可选，多数情况下可为空，用于避免自动获取 World。 */
	template <typename T = ThisClass>
	static FORCEINLINE T& Get(TSubclassOf<UPoolManagerSubsystem> OptionalClass = T::StaticClass(), const UObject* OptionalWorldContext = nullptr) { return *CastChecked<T>(GetPoolManagerByClass(OptionalClass, OptionalWorldContext)); }

	/** 返回 Pool Manager 指针。
	 * 供蓝图获取 **默认** 的 Pool Manager。
	 * @param OptionalWorldContext 可选参数且对蓝图隐藏，多数情况下可为空，用于避免自动获取 World。 */
	UFUNCTION(BlueprintPure, meta = (WorldContext = "OptionalWorldContext", CallableWithoutWorldContext))
	static UPoolManagerSubsystem* GetPoolManager(const UObject* OptionalWorldContext = nullptr) { return GetPoolManagerByClass(StaticClass(), OptionalWorldContext); }

	/** 按指定类返回自定义 Pool Manager 指针。
	 * 供蓝图获取 **自定义** 的 Pool Manager。
	 * @param OptionalClass 可选，若实现了自己的 Pool Manager，指定其类。
	 * @param OptionalWorldContext 可选参数且对蓝图隐藏，多数情况下可为空，用于避免自动获取 World。 */
	UFUNCTION(BlueprintPure, meta = (WorldContext = "OptionalWorldContext", CallableWithoutWorldContext, DeterminesOutputType = "OptionalClass", BlueprintAutocast))
	static UPoolManagerSubsystem* GetPoolManagerByClass(TSubclassOf<UPoolManagerSubsystem> OptionalClass = nullptr, const UObject* OptionalWorldContext = nullptr);

	/*********************************************************************************************
	 * 从池中取出（单个对象）
	 * 用它来获取对象，而不是自己手动创建。
	 ********************************************************************************************* */
public:
	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnTakenFromPool, UObject*, Object);

	/** 按指定类从池中获取单个对象，输出为异步回调，对象就绪后返回。
	 *  若池中没有空闲对象或该池尚不存在，则创建新对象。
	 *  @param ObjectClass 要从池中获取的对象类。
	 *  @param Transform 对象的 Transform（仅对 Actor 生效）。
	 *  @param Completed 对象就绪时触发的回调输出。
	 *  @param Priority 请求优先级，优先级越高越先生成。
	 *  @return 若找到空闲对象则激活并返回；否则在后续帧异步生成新对象并注册到池中。
	 *  @warning 仅供蓝图使用：在代码中请改用 TakeFromPool()。
	 *  - 'SpawnObjectsPerFrame' 决定新对象的生成速度，可在 'Project Settings' -> "Plugins" -> "Pool Manager" 中修改。
	 *  - 该节点是 K2Node_TakeFromPool.h 实现的自定义蓝图节点，因此无法被重写，且只能放在事件图表（不能放在函数内）。
	 *  - 需要多个对象时请用 BPTakeFromPoolArray，不要在 for/while 中逐个请求：'Completed' 输出在循环中不生效。 */
	UFUNCTION(BlueprintCallable, Category = "[Pool Manager]", DisplayName = "Take From Pool", meta = (BlueprintInternalUseOnly = "true", AutoCreateRefTerm = "Transform"))
	void BPTakeFromPool(const UClass* ObjectClass, const FTransform& Transform, const FOnTakenFromPool& Completed, ESpawnRequestPriority Priority);

	/** BPTakeFromPool() 的代码版本，可被重写，对象就绪时调用回调函数。
	 * 可由子类在代码中重写。
	 * 适合在代码中处理蓝图类，例如：TakeFromPool(SomeBlueprintClass);
	 * @return 与对象关联的 Handle（内含 Hash）。由于对象可能尚未就绪，因此返回的是间接引用。 */
	virtual struct FPoolObjectHandle TakeFromPool(const UClass* ObjectClass, const FTransform& Transform = FTransform::Identity, const FOnSpawnCallback& Completed = nullptr, ESpawnRequestPriority Priority = ESpawnRequestPriority::Normal);

	/** TakeFromPool() 的模板版本，通过模板参数指定对象类。
	 * 适合在代码中处理 C++ 类，例如：TakeFromPool<AProjectile>(); */
	template <typename T>
	struct FPoolObjectHandle TakeFromPool(const FTransform& Transform = FTransform::Identity, const FOnSpawnCallback& Completed = nullptr, ESpawnRequestPriority Priority = ESpawnRequestPriority::Normal) { return TakeFromPool(T::StaticClass(), Transform, Completed, Priority); }

	/** TakeFromPool() 的同步版本：在池中查找可用对象，找不到则返回 null。 */
	virtual const struct FPoolObjectData* TakeFromPoolOrNull(const UClass* ObjectClass, const FTransform& Transform = FTransform::Identity);

	/*********************************************************************************************
	 * 从池中取出（多个对象）
	 * 需要一次获取多个对象时，用它替代单对象版本。
	 ********************************************************************************************* */
public:
	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnTakenFromPoolArray, const TArray<UObject*>&, Objects);

	/** 与 BPTakeFromPool() 相同，但用于批量获取对象。
	 * @param ObjectClass 要从池中获取的对象类。
	 * @param Amount 要获取的对象数量。
	 * @param Completed 所有对象就绪后触发的回调输出。
	 * @param Priority 请求优先级，优先级越高越先生成。
	 * @warning 仅供蓝图使用：在代码中请改用 TakeFromPoolArray()。 */
	UFUNCTION(BlueprintCallable, Category = "[Pool Manager]", DisplayName = "Take From Pool Array", meta = (BlueprintInternalUseOnly = "true"))
	void BPTakeFromPoolArray(const UClass* ObjectClass, int32 Amount, const FOnTakenFromPoolArray& Completed, ESpawnRequestPriority Priority);

	/** BPTakeFromPoolArray() 的代码版本，可被重写，同类对象全部就绪时调用回调函数。 */
	virtual void TakeFromPoolArray(TArray<struct FPoolObjectHandle>& OutHandles, const UClass* ObjectClass, int32 Amount, const FOnSpawnAllCallback& Completed = nullptr, ESpawnRequestPriority Priority = ESpawnRequestPriority::Normal);

	/** TakeFromPoolArray() 的另一个版本，可一次处理不同类、不同 Transform 的多个请求。
	 * @param OutHandles 返回与待生成对象关联的 Handle（对象将在后续帧生成）。
	 * @param InRequests 输入的类与 Transform 列表。
	 * @param Completed 所有对象就绪后调用一次的回调函数。 */
	virtual void TakeFromPoolArray(TArray<struct FPoolObjectHandle>& OutHandles, TArray<struct FSpawnRequest>& InRequests, const FOnSpawnAllCallback& Completed = nullptr);

	/** TakeFromPoolArray() 的同步版本：在池中批量查找可用对象，找不到则不返回。
	 * @param OutObjects 已找到并取出的对象；若当前没有可用对象则为空数组。
	 * @param InRequests 输入的类与 Transform 列表。 */
	virtual void TakeFromPoolArrayOrNull(TArray<struct FPoolObjectData>& OutObjects, TArray<struct FSpawnRequest>& InRequests);

	/*********************************************************************************************
	 * 归还到池（单个对象）
	 * 用它归还对象，而不是自己手动销毁。
	 ********************************************************************************************* */
public:
	/** 将指定对象归还到池中；若该对象此前是从池中取出的，则同时将其置为非激活。
	 * @param Object 要归还的对象。
	 * @return 找到对应的池并归还成功返回 true，否则返回 false。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "[Pool Manager]", meta = (DefaultToSelf = "Object"))
	bool ReturnToPool(UObject* Object);
	virtual bool ReturnToPool_Implementation(UObject* Object);

	/** ReturnToPool() 的替代版本：按 Handle 归还对象。
	 * 按 Handle 归还比按对象归还更可靠，因为对象可能尚未就绪（还在生成队列中）。
	 * 适用于没有对象引用但持有其 Handle 的场景。
	 * @param Handle 与待归还对象关联的 Handle。
	 * @return 找到该 Handle 并归还成功返回 true，否则返回 false。 */
	virtual bool ReturnToPool(const struct FPoolObjectHandle& Handle);

	/*********************************************************************************************
	 * 归还到池（多个对象）
	 * 需要一次归还多个对象时，用它替代单对象版本。
	 ********************************************************************************************* */
public:
	/** 与 ReturnToPool() 相同，但用于批量归还对象。*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "[Pool Manager]")
	bool ReturnToPoolArray(const TArray<UObject*>& Objects);
	virtual bool ReturnToPoolArray_Implementation(const TArray<UObject*>& Objects);

	/** 与 ReturnToPool() 相同，但用于批量归还 Handle。 */
	virtual bool ReturnToPoolArray(const TArray<struct FPoolObjectHandle>& Handles);

	/*********************************************************************************************
	 * 进阶接口
	 * 多数情况下不需要使用本节的接口。
	 ********************************************************************************************* */
public:
	/** 按对象所属类，将指定对象原样加入池中，交由 Pool Manager 管理。
	 * 多数情况下不应直接调用，该函数会被自动调用。
	 * 适用于把已经存在（由外部代码生成）的对象纳入池管理。
	 * 仅用于 Pool Manager 尚不知晓的已存在对象。
	 * @param InData 要注册到池中的对象数据。
	 * @param bNotify 默认为 true；为 false 时直接修改状态而不触发回调。
	 * @return 注册成功返回 true，否则返回 false。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "[Pool Manager]", meta = (AutoCreateRefTerm = "InData"))
	bool RegisterObjectInPool(const struct FPoolObjectData& InData, bool bNotify = true);
	virtual bool RegisterObjectInPool_Implementation(const struct FPoolObjectData& InData, bool bNotify = true);

	/** 始终创建新对象并按其类加入池中。
	 * 请谨慎使用，仅在池中确实没有空闲对象时才调用。
	 * @param InRequest 生成新对象的请求。
	 * @return 与待生成对象关联的 Handle（内含 hash），对象将在后续帧生成。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "[Pool Manager]", meta = (AutoCreateRefTerm = "InRequest"))
	struct FPoolObjectHandle CreateNewObjectInPool(const struct FSpawnRequest& InRequest);
	virtual struct FPoolObjectHandle CreateNewObjectInPool_Implementation(const struct FSpawnRequest& InRequest);

	/** 与 CreateNewObjectInPool() 相同，但用于批量创建对象。 */
	virtual void CreateNewObjectsArrayInPool(TArray<struct FSpawnRequest>& InRequests, TArray<struct FPoolObjectHandle>& OutAllHandles, const FOnSpawnAllCallback& Completed = nullptr);

	/*********************************************************************************************
	 * 进阶接口 - 工厂
	 ********************************************************************************************* */
public:
	/** 注册新的 Factory，供 Pool Manager 处理指定类及其子类的对象时使用。
	 * 多数情况下无需调用，Factory 会被自动注册。
	 * 适用于注册来自其他插件或模块的 Factory。 */
	UFUNCTION(BlueprintCallable, Category = "[Pool Manager]")
	virtual void AddFactory(TSubclassOf<UPoolFactory_UObject> FactoryClass);

	/** 按类移除 Pool Manager 中的 Factory。
	 * 多数情况下无需调用，Factory 会随 Pool Manager 销毁一并移除。
	 * 适用于运行时某个类不再需要时移除对应 Factory。 */
	UFUNCTION(BlueprintCallable, Category = "[Pool Manager]")
	virtual void RemoveFactory(TSubclassOf<UPoolFactory_UObject> FactoryClass);

	/** 沿类继承链向上查找，返回处理该对象类型（或其祖先类）的最近一个已注册 Factory。 */
	UFUNCTION(BlueprintPure, Category = "[Pool Manager]")
	UPoolFactory_UObject* FindPoolFactoryChecked(const UClass* ObjectClass) const;

	/** 返回指定 Factory 所负责处理的默认对象类。 */
	UFUNCTION(BlueprintPure, Category = "[Pool Manager]")
	static const UClass* GetObjectClassByFactory(const TSubclassOf<UPoolFactory_UObject>& FactoryClass);

protected:
	/** 创建 Pool Manager 处理对象时可能用到的全部 Pool Factory。 */
	virtual void InitializeAllFactories();

	/** 销毁 Pool Manager 处理对象时使用的全部 Pool Factory。 */
	virtual void ClearAllFactories();

	/*********************************************************************************************
	 * 清空池
	 ********************************************************************************************* */
public:
	/** 销毁指定类对应池中的所有对象。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void EmptyPool(const UClass* ObjectClass);
	virtual void EmptyPool_Implementation(const UClass* ObjectClass);

	/** 销毁 Pool Manager 管理的所有池中的所有对象。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void EmptyAllPools();
	virtual void EmptyAllPools_Implementation();

	/** 按谓词（Predicate）销毁 Pool Manager 中符合条件的所有对象。 */
	virtual void EmptyAllByPredicate(const TFunctionRef<bool(const UObject* PoolObject)> Predicate);

	/*********************************************************************************************
	 * 查询接口
	 ********************************************************************************************* */
public:
	/** 返回指定对象当前的状态。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "[Pool Manager]", meta = (DefaultToSelf = "Object"))
	EPoolObjectState GetPoolObjectState(const UObject* Object) const;
	virtual EPoolObjectState GetPoolObjectState_Implementation(const UObject* Object) const;

	/** 指定对象是否由 Pool Manager 管理。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "[Pool Manager]", meta = (DefaultToSelf = "Object"))
	bool ContainsObjectInPool(const UObject* Object) const;
	virtual bool ContainsObjectInPool_Implementation(const UObject* Object) const;

	/** 指定类是否由 Pool Manager 管理。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "[Pool Manager]")
	bool ContainsClassInPool(const UClass* ObjectClass) const;
	virtual bool ContainsClassInPool_Implementation(const UClass* ObjectClass) const;

	/** 指定对象是否由 Pool Manager 管理且已从池中取出。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "[Pool Manager]", meta = (DefaultToSelf = "Object"))
	bool IsActive(const UObject* Object) const;
	virtual bool IsActive_Implementation(const UObject* Object) const;

	/** 受管对象是否处于非激活状态、可被取出。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "[Pool Manager]", meta = (DefaultToSelf = "Object"))
	bool IsFreeObjectInPool(const UObject* Object) const;
	virtual bool IsFreeObjectInPool_Implementation(const UObject* Object) const;

	/** 返回指定类的池中空闲对象数量。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "[Pool Manager]")
	int32 GetFreeObjectsNum(const UClass* ObjectClass) const;
	virtual int32 GetFreeObjectsNum_Implementation(const UClass* ObjectClass) const;

	/** 指定对象是否已被 Pool Manager 记录。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "[Pool Manager]", meta = (DefaultToSelf = "Object"))
	bool IsRegistered(const UObject* Object) const;
	virtual bool IsRegistered_Implementation(const UObject* Object) const;

	/** 返回指定类的池中已注册对象数量。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "[Pool Manager]")
	int32 GetRegisteredObjectsNum(const UClass* ObjectClass) const;
	virtual int32 GetRegisteredObjectsNum_Implementation(const UClass* ObjectClass) const;

	/** 对象是否有效且已注册到池中。 */
	UFUNCTION(BlueprintPure, Category = "[Pool Manager]", meta = (AutoCreateRefTerm = "InPoolObject"))
	static bool IsPoolObjectValid(const struct FPoolObjectData& InPoolObject) { return InPoolObject.IsValid(); }

	/** 返回与给定 Handle 关联的对象。
	 * 若未找到，或对象仍在生成队列中，则返回无效的 PoolObject。 */
	UFUNCTION(BlueprintPure, Category = "[Pool Manager]")
	const struct FPoolObjectData& FindPoolObjectByHandle(const struct FPoolObjectHandle& Handle) const;

	/** 返回与给定对象关联的 Handle。
	 * 未找到时返回无效 Handle（FPoolObjectHandle::EmptyHandle）。 */
	UFUNCTION(BlueprintPure, Category = "[Pool Manager]", meta = (DefaultToSelf = "Object"))
	const struct FPoolObjectHandle& FindPoolHandleByObject(const UObject* Object) const;

	/** 从给定的所有 Handle 中筛选出有效的对象。 */
	UFUNCTION(BlueprintPure, Category = "[Pool Manager]")
	void FindPoolObjectsByHandles(TArray<struct FPoolObjectData>& OutObjects, const TArray<struct FPoolObjectHandle>& InHandles) const;

	/*********************************************************************************************
	 * 受保护属性
	 ********************************************************************************************* */
protected:
	/** Pool Manager 管理的全部池。 */
	UPROPERTY(BlueprintReadWrite, Transient, Category = "[Pool Manager]", meta = (BlueprintProtected))
	TArray<FPoolContainer> Pools;

	/** 已注册的 Factory 与其负责处理的类之间的映射。
	 * @see UPoolFactory_UObject 的类说明。 */
	UPROPERTY(BlueprintReadWrite, Transient, Category = "[Pool Manager]", meta = (BlueprintProtected))
	TMap<TObjectPtr<const UClass>, TObjectPtr<UPoolFactory_UObject>> AllFactories;

	/*********************************************************************************************
	 * 受保护方法
	 ********************************************************************************************* */
protected:
	//~Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~End USubsystem Interface

	//~Begin IGameFeatureStateChangeObserver Interface
	/** 仅用于启用 Modular Game Features 的项目：自动清空归属于正在停用插件的那些类的池，
	 * 因此无需手动调用 EmptyPool */
	virtual void OnGameFeatureDeactivating(const UGameFeatureData* GameFeatureData, FGameFeatureDeactivatingContext& Context, const FString& PluginURL) override;
	//~End IGameFeatureStateChangeObserver Interface

	/** 按指定类返回找到的池指针。 */
	virtual FPoolContainer& FindPoolOrAdd(const UClass* ObjectClass);
	virtual FPoolContainer* FindPool(const UClass* ObjectClass);
	const FORCEINLINE FPoolContainer* FindPool(const UClass* ObjectClass) const { return const_cast<UPoolManagerSubsystem*>(this)->FindPool(ObjectClass); }

	/** 若对象由 Pool Manager 管理，则激活或停用该对象。
	 * 在对象被取出、注册或归还到池时调用。
	 * @param NewState 为 true 时激活对象，否则停用。
	 * @param InObject 要激活或停用的对象。
	 * @param InPool 包含该对象的池。
	 * @param bNotify 默认为 true；为 false 时直接修改状态而不触发回调。
	 * @warning 不要直接调用，请改用 TakeFromPool() 或 ReturnToPool()。 */
	virtual void SetObjectStateInPool(EPoolObjectState NewState, UObject& InObject, FPoolContainer& InPool, bool bNotify = true);

	/**
	 * 将对象从池的注册表中注销，并从内部池数据中移除。
	 *
	 * 这会把对象从 Pool Manager 中永久移除，适用于该对象不应再参与池化的场景
	 * （例如达到缓存上限）。
	 *
	 * 与 SetObjectStateInPool() 不同，本函数不改变激活/非激活状态，
	 * 而是把对象彻底从池化系统中剥离。
	 *
	 * @warning 不要在 gameplay 代码中直接调用。
	 * 仅由 Pool Manager 在需要销毁或丢弃对象时内部使用。
	 */
	virtual void RemoveObjectInPool(UObject& InObject, FPoolContainer& InPool);
};
