# OUUTags Plugin (project plugin, v1.0.0)

Type-safe extensions over GameplayTags. Modules: `OUUTags` (Runtime), `OUUTagsEditor` (validation, detail customization), `OUUTagsTests`. Log category `LogOUUTags`.

## Literal tags (compile-time hierarchy, auto-registered as native tags)

```cpp
// .h
OUU_DECLARE_GAMEPLAY_TAGS_START(MYGAME_API, FMyTags, "Game", "desc")
    OUU_GTAG_GROUP_START(Ability, "group")
        OUU_GTAG(Active, "leaf")
    OUU_GTAG_GROUP_END(Ability)
OUU_DECLARE_GAMEPLAY_TAGS_END(FMyTags)
// .cpp
OUU_DEFINE_GAMEPLAY_TAGS(FMyTags)
// use
FGameplayTag Tag = FMyTags::Ability::Active::GetTag();   // GetName() for the FName
```

Other modules append with `OUU_DECLARE_GAMEPLAY_TAGS_EXTENSION_START(API, NewType, ExistingType)`. Flags (`ELiteralGameplayTagFlags`): `AutoRegister` (default), `AllowContentChildTags`, `Inherited`, `Explicit`.

## Typed tags

`IMPLEMENT_TYPED_GAMEPLAY_TAG(FAbilityTag, FMyTags::Ability)` restricts a tag type to children of the given literal roots. `TTypedGameplayTag`: `TryConvert` (optional), `ConvertChecked` (asserts), `GetAllRootTags`, `GetAllLeafTags`. Containers mirror `FGameplayTagContainer`: `TTypedGameplayTagContainerValue<T>` (return values), `TTypedGameplayTagContainerReference<T>` (UPROPERTY). `FTypedGameplayTagContainer::Create<T>(Initial)` is the runtime-checked form for UPROPERTY with a filtered picker; `UTypedGameplayTagContainerLibrary` exposes it to Blueprint. Extra roots per type are configured in Project Settings → OUU Tags → Typed Gameplay Tag Settings (`UTypedGameplayTagSettings`).

## Other pieces

- `UOUUGameplayTagLibrary`: `GetParentTag`, `GetChildTags(Tag, MaxDepth)`, `GetTagDepth`, `GetTagUntilDepth`, `GetTagComponents`/`CreateTagFromComponents`.
- `IGameplayTagDependencyInterface`: an object's tags include those of objects added with `AddDependency`; `AppendOwnTags` is the override point, `BindEventToOnTagsChanged` listens (`FGameplayTagDependencyChange`), `GetOriginalTagSources` traces the root holder.
- `FGameplayTagQueryParser::ParseQuery("ANY(ALL(A.B, C.D), E)")` builds an `FGameplayTagQuery` from text (`ANY`/`ALL`/`NOT`).

## Editor validation (`UGameplayTagValidationSettings`, Project Settings → OUU Tags)

Rules: `MaxGlobalTagDepth` (10), `NativeTagAllowedChildDepth` (3), `bAllowContentRootTags`, `bDefaultAllowContentTagChildren`, validate after startup / tree change / cook, `WarnOnlyGameplayTags` allowlist. `UGameplayTagValidatorSubsystem::ValidateGameplayTagTree()` runs it manually; derive `UGameplayTagValidator_Base` (`InitializeValidator`, `ValidateTag`) for custom rules.

## Project constraints

- Its config section is `OUUTagsEditor`, not `OUUTags`, and it must not occupy the `GameplayTags` config: doing so crashes on an engine whitelist ensure when adding tags (no crash report in Debug; re-apply after upstream sync).
- Noisy validator output when adding tags does not block writing; `DefaultOUUTags.ini` is already relaxed.
