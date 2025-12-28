# Praxis StateTree Development Log

## Session: 2024-12-28 - StateTree Context Integration

### Overview
Implementing StateTree-based machine behavior system to replace legacy PraxisMass task architecture. Goal is to create a component-based approach where StateTree tasks can access and modify machine state through MachineContextComponent.

---

## Timeline

### 1. Initial Context Attempt (Failed - UHT Issues)
**Time:** Early session  
**Request:** Create StateTree context system with component binding  
**Actions:**
- Created `FPraxisMachineContext` struct in PraxisSimulationKernel module
- Created `UMachineContextComponent` wrapper
- Created StateTree tasks (Production, Changeover, Idle, etc.)
- Created conditions and evaluators

**Result:** ❌ **FAILED**
- Persistent UHT error: `Cannot open include file: 'PraxisMachineContext.generated.h'`
- Multiple troubleshooting attempts:
  - Forward declarations (failed - can't forward declare UPROPERTY types)
  - Simplified context structure (removed FPraxisWorkOrder dependency)
  - File relocation (moved from nested directories to root Public/)
  - File renaming (PraxisMachineContext → MachineContext)
  - Clean rebuilds (deleted Intermediate multiple times)
- Root cause: Likely cross-module dependency issues between PraxisSimulationKernel and PraxisCore
- Never resolved - decided to roll back

**Files Created (all disabled with .zzz extensions):**
- PraxisMachineContext.h/.cpp
- MachineContextComponent.h/.cpp (old version)
- STTask_Production, STTask_Changeover, STTask_JamRecovery
- STCondition_CheckForJam, STCondition_HasWorkOrder
- PraxisEvaluator_QualityOutcome, PraxisEvaluator_RandomJam

**Commit:** "Rollback: Remove incomplete StateTree context implementation"

---

### 2. Investigation: RiderLink Build Issue
**Time:** Mid-session  
**Request:** Investigate RiderLink build failure after Rider update  
**Actions:**
- Analyzed UBA-UnrealEditor-Win64-Development.txt log
- Traced through compilation steps

**Result:** ✅ **RESOLVED**
- RiderLink actually built successfully (all 125 steps completed)
- The failure was from the main Praxis project (PraxisMachineContext errors)
- Once project rolled back, RiderLink worked fine

---

### 3. Fresh Start: Context in PraxisCore (Success)
**Time:** Mid-session  
**Request:** Implement StateTree context with lessons learned from failure  
**Actions:**

**Phase 1: Create Context in PraxisCore**
- Created `FPraxisMachineContext` in `PraxisCore/Public/FPraxisMachineContext.h`
  - Simple USTRUCT with POD types only
  - No cross-module dependencies
  - Stores: MachineId, ProductionRate, config values, runtime counters, work order data
- **Why PraxisCore?** Eliminates cross-module header issues

**Phase 2: Component Wrapper**
- Created `UMachineContextComponent` in PraxisSimulationKernel
  - Simple wrapper around FPraxisMachineContext
  - Provides `GetContext()` and `GetMutableContext()` accessors
  - `InitializeContext()` method for setup

**Phase 3: Update MachineLogicComponent**
- Modified to create both StateTreeComponent AND MachineContextComponent
- Components created in `OnRegister()`
- Context initialized in `BeginPlay()`
- Subscribes to Orchestrator events
- Manually ticks StateTree on sim tick

**Result:** ✅ **SUCCESS - Compiles cleanly**

**Commit:** "Add StateTree context foundation in PraxisCore"

---

### 4. Test Task Creation
**Time:** Mid-session  
**Request:** Create simple test task to verify binding works  
**Actions:**

**Initial Attempt:**
- Created `STTask_TestIncrement` task
- Simple task that increments OutputCounter and logs every 10 ticks
- Used standard component binding pattern

**Problem:** Task not appearing in StateTree editor
**Solution:** Added `Category = "Praxis"` metadata to USTRUCT
- StateTree tasks require Category metadata to appear in editor picker
- Thanks to Junie for identifying this!

**Result:** ✅ Task appears in editor under "Praxis" category

**Commit:** "Add test task to verify StateTree binding"

---

### 5. Runtime Binding Issues
**Time:** Late session  
**Request:** Fix component binding in StateTree editor  
**Actions:**

**Problem 1:** StateTree wouldn't start - "Ticking a paused or a not running State Tree component"
**Investigation:**
- StateTree reported "started" in logs
- But immediately stopped/failed
- Binding showed `MachineContext=None` in StateTree asset

**Problem 2:** StateTree editor required binding but couldn't bind
**Error:** "Input property 'MachineContext' is expected to have a binding"

**Solution:**
1. Made MachineContext binding optional: `meta = (Optional)`
2. Implemented auto-discovery: Task finds component on owner Actor if not bound
3. Added runtime fallback in `EnterState()`:
   ```cpp
   if (!InstanceData.MachineContext)
   {
       if (AActor* Owner = Cast<AActor>(Context.GetOwner()))
       {
           InstanceData.MachineContext = Owner->FindComponentByClass<UMachineContextComponent>();
       }
   }
   ```

**Result:** ✅ **WORKS PERFECTLY**
- Task finds MachineContextComponent automatically
- Counter increments successfully (reached 980+ in test run)
- Logs show proper tick execution with DeltaTime
- Foundation proven solid

**Commit:** "StateTree context binding verified - test task works"

---

## Current State (End of Session)

### ✅ Working Components
1. **FPraxisMachineContext** (in PraxisCore)
   - Contains all machine state (config, runtime counters, work order data)
   - No cross-module issues
   - Clean USTRUCT with UHT-friendly types

2. **UMachineContextComponent** (in PraxisSimulationKernel)
   - Wraps FPraxisMachineContext
   - Provides access to StateTree tasks
   - Automatically discovered by tasks

3. **UMachineLogicComponent**
   - Creates both StateTreeComponent and MachineContextComponent
   - Integrates with Orchestrator (manual ticking)
   - Manages lifecycle properly

4. **STTask_TestIncrement**
   - Proven working test task
   - Auto-discovers component binding
   - Successfully modifies machine state

### 📁 Active Files
```
PraxisCore/Public/FPraxisMachineContext.h
PraxisSimulationKernel/Public/Components/MachineContextComponent.h
PraxisSimulationKernel/Private/Components/MachineContextComponent.cpp
PraxisSimulationKernel/Public/Components/MachineLogicComponent.h
PraxisSimulationKernel/Private/Components/MachineLogicComponent.cpp
PraxisSimulationKernel/Public/StateTrees/Tasks/STTask_TestIncrement.h
PraxisSimulationKernel/Private/StateTrees/Tasks/STTask_TestIncrement.cpp
```

### 🗃️ Archived Files (.zzz extension)
All original StateTree implementation files from failed attempt preserved for reference.

---

## Key Lessons Learned

1. **Cross-Module UHT Issues:** 
   - USTRUCT types with UPROPERTYs should be in the module where they're primarily used
   - Avoid complex cross-module header dependencies in reflection code
   - PraxisCore → PraxisSimulationKernel works; reverse does not

2. **StateTree Editor Discovery:**
   - Tasks need `Category` metadata in USTRUCT to appear in editor
   - Without it, UHT generates code but editor can't find the task

3. **Component Binding:**
   - Mark bindings as `Optional` if auto-discovery fallback exists
   - Use `FindComponentByClass()` for robust runtime binding
   - StateTree's `Context.GetOwner()` returns `UObject*` - must cast to `AActor*`

4. **Incremental Development:**
   - One test task proved the entire foundation
   - Commit frequently (multiple times per session)
   - Don't create 5+ tasks at once - validate incrementally

---

## Next Steps

### Immediate (Next Session)
1. Create **STTask_Idle** 
   - Simplest production task
   - Waits for work order assignment
   - Transitions when `bHasActiveWorkOrder` becomes true

2. Create **STTask_Production**
   - Core production logic
   - Accumulates production based on ProductionRate × DeltaTime
   - Rolls scrap/good based on ScrapRate
   - Completes when OutputCounter >= TargetQuantity

3. Create **STTask_Changeover**
   - Setup/transition task
   - Counts down ChangeoverDuration
   - Transitions to Production when complete

### Future Tasks
4. **STCondition_HasWorkOrder** - Transition condition
5. **STCondition_CheckForJam** - Probabilistic failure check
6. **STTask_JamRecovery** - Handles machine jams
7. **Evaluators** - QualityOutcome, RandomJam (if needed)

### Architecture Notes
- Each task should follow the TestIncrement pattern:
  - Optional component binding with auto-discovery
  - Category = "Praxis" for editor visibility
  - Proper error handling if component not found
  - Commit after each working task

---

## Technical Decisions

### Why PraxisCore for Context?
- Avoids circular dependencies
- Both PraxisCore and PraxisSimulationKernel can include it
- UHT has no issues processing it there

### Why Auto-Discovery?
- StateTree component binding can be finicky in editor
- Runtime fallback ensures robustness
- Reduces user error (no binding required in editor)

### Why Manual StateTree Ticking?
- Orchestrator controls simulation time
- Deterministic 5-second ticks
- StateTree must sync with sim time, not frame time

---

## Build Configuration

### Module Dependencies (PraxisSimulationKernel.Build.cs)
```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core",
    "AIModule",
    "PraxisCore",
    "StateTreeModule",
    "GameplayStateTreeModule",
    "GameplayTags"
});
```

**Note:** Attempted to add `StateTreeEditorModule` but it caused build errors. Not needed for runtime.

---

## Current Metrics
- **Session Duration:** ~3-4 hours
- **Commits:** 3 major commits
  - Rollback commit (clean slate)
  - Foundation commit (context + component)
  - Test task commit (verification)
- **Lines of Code:** ~500 (active), ~2000+ (archived)
- **Build Status:** ✅ Clean compile
- **Runtime Status:** ✅ Fully functional test

---

## End of Session Summary

**Status:** 🟢 **FOUNDATION COMPLETE & VERIFIED**

We successfully implemented a StateTree context system after learning from an initial failed approach. The key was moving the context struct to PraxisCore to avoid cross-module header issues, and implementing robust component auto-discovery to handle StateTree's binding quirks.

The test task proves the architecture works. We're now ready to build production tasks incrementally, one at a time, with frequent commits.

**Ready for next session:** Implement STTask_Idle (simplest production task)
