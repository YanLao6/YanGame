/**
 * 角色动画数据层共用类型：枚举状态轴与状态历史结构。
 *
 * 动画层（UYanCharacterAnimInstance）只通过 AYanDefaultPawn 上一组 BlueprintPure getter
 * 读取运动学状态——这组 getter 即 Pawn→动画层的约定契约，动画层不承担运动解算。
 */

/** 移动大类：贴地 / 腾空 */
enum EYanMovementMode
{
	Grounded,
	InAir
}

/** 朝向模式：面向移动方向 / 锁定朝向侧移（瞄准姿态后续再扩展 Aim） */
enum EYanRotationMode
{
	OrientToMovement,
	Strafe
}

/** 运动状态：静止 / 移动中 */
enum EYanMovementState
{
	Idle,
	Moving
}

/** 步态档位，由水平速度阈值裁决 */
enum EYanGait
{
	Walk,
	Run,
	Sprint
}

/** 姿态：站立 / 蹲伏 */
enum EYanStance
{
	Stand,
	Crouch
}

/** 相对角色朝向的移动方向象限，仅 Strafe 下有意义 */
enum EYanMovementDirection
{
	Forward,
	Backward,
	Left,
	Right
}

/**
 * 单条枚举状态轴的历史记录。所有状态轴共用同一套更新逻辑：
 * Current 为当前值；切换时把旧值与其停留时长存入 Recent / LastStateDuration，
 * 当前值停留超过 RecentThreshold 后 Recent 收敛到 Current（不再视为「刚切换」）。
 * 枚举以 int 存储，读取侧用对应 EYan* 强转。
 */
struct FYanStateHistory
{
	UPROPERTY(BlueprintReadOnly)
	int Current;

	UPROPERTY(BlueprintReadOnly)
	int LastFrame;

	UPROPERTY(BlueprintReadOnly)
	int Recent;

	/** 当前值已持续的时长（秒） */
	UPROPERTY(BlueprintReadOnly)
	float TimeInState;

	/** 上一个值持续了多久（秒） */
	UPROPERTY(BlueprintReadOnly)
	float LastStateDuration;

	void Update(int NewValue, float DeltaSeconds, float RecentThreshold)
	{
		LastFrame = Current;

		if (NewValue != Current)
		{
			Recent = Current;
			LastStateDuration = TimeInState;
			Current = NewValue;
			TimeInState = 0.f;
			return;
		}

		TimeInState += DeltaSeconds;
		if (TimeInState > RecentThreshold)
		{
			Recent = Current;
		}
	}
}
