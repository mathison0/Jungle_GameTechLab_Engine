# Tank Weapon Lua System Guide

이 문서는 현재 프로젝트 기준으로 탱크 무기 시스템을 수정하거나 새 무기를 추가할 때 팀원이 확인해야 할 위치를 정리한 문서다.

현재 구조의 핵심은 **무기 데이터 / 외형 배치 / 런타임 동작 / C++ 실행 API를 분리하는 것**이다.

```txt
수치 데이터      -> Asset/Scripts/WeaponDefs.lua
외형/머즐 배치   -> Asset/Scripts/WeaponVisualDefs.lua
무기 동작 로직   -> Asset/Scripts/Weapons/<WeaponName>Weapon.lua
무기 등록/업글   -> Asset/Scripts/WeaponInventory.lua
레벨업 선택      -> Asset/Scripts/LevelSystem.lua
발사체 런타임    -> Asset/Scripts/ProjectileScript.lua
데미지 전달      -> Asset/Scripts/Core/DamageSystem.lua
적 체력 처리     -> Asset/Scripts/Components/EnemyState.lua
```

`ATankActor`는 무기 레벨, 경험치, 인벤토리, 쿨타임, 타겟 선택을 직접 관리하지 않는다.  
C++ 탱크는 Lua가 넘긴 요청을 받아 **외형 생성, 머즐 조회, 발사체/미사일 생성, 이펙트용 알림**만 처리한다.

---

## 1. 변경 목적별 수정 위치

### 데미지, 공속, 사거리, 포탑 수, 공격 타입 변경

수치 데이터는 여기서 수정한다.

```txt
Asset/Scripts/WeaponDefs.lua
```

예시:

```lua
WeaponDefs.MachineTurret = {
    DisplayName = "Machine Turret",
    AttackType = "InstantTargetDamage",
    MaxLevel = 3,
    Levels = {
        [1] = {
            Damage = 3,
            FireInterval = 1.0,
            TurretCount = 1,
            Range = 10000.0,
            TargetRefreshInterval = 0.2,
        },
        [2] = {
            Damage = 3,
            FireInterval = 1.0,
            TurretCount = 2,
            Range = 10000.0,
            TargetRefreshInterval = 0.2,
        },
        [3] = {
            Damage = 4,
            FireInterval = 0.35,
            TurretCount = 4,
            Range = 10000.0,
            TargetRefreshInterval = 0.15,
        },
    },
}
```

`WeaponDefs.lua`에는 위치, 스케일, 메시 경로 같은 외형 정보를 넣지 않는다.

---

### 무기 외형, 부착 위치, 머즐 위치 변경

외형 배치는 여기서 수정한다.

```txt
Asset/Scripts/WeaponVisualDefs.lua
```

여기에는 레벨별로 어떤 컴포넌트를 탱크에 붙일지 정의한다.

```lua
WeaponVisualDefs.MachineTurret = {
    [3] = {
        Visuals = {
            {
                Name = "Visual_MachineTurret_3",
                Mesh = "Models/_Basic/Cube.OBJ",
                Parent = "RootComponent",
                Location = { -0.7, -0.8, 0.8 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 0.6, 0.35, 0.35 },
                MuzzleName = "Muzzle_MachineTurret_3",
                MuzzleLocation = { 0.9, 0.0, 0.0 },
            },
        },
    },
}
```

C++ `ATankActor::EquipWeaponVisualFromLua()`는 이 Lua table을 읽어서 컴포넌트를 생성하거나 재사용한다.

이름 규칙은 반드시 지킨다.

```txt
Visual_<WeaponId>_<Index>
Muzzle_<WeaponId>_<Index>
```

예시:

```txt
Visual_MainCannon_0
Muzzle_MainCannon_0
Visual_MachineTurret_0
Muzzle_MachineTurret_0
Visual_MachineTurret_3
Muzzle_MachineTurret_3
Visual_HomingMissile_0
Muzzle_HomingMissile_0
```

머즐이 없으면 C++은 fallback하지 않고 warning 로그를 남긴 뒤 return해야 한다.

---

### 무기 동작 로직 변경

무기의 실제 동작은 여기서 수정한다.

```txt
Asset/Scripts/Weapons/<WeaponName>Weapon.lua
```

예시:

```txt
Asset/Scripts/Weapons/MainCannonWeapon.lua
Asset/Scripts/Weapons/MachineTurretWeapon.lua
Asset/Scripts/Weapons/AuraWeapon.lua
Asset/Scripts/Weapons/HomingMissileWeapon.lua
Asset/Scripts/Weapons/SatelliteBeamWeapon.lua
```

여기에는 다음이 들어간다.

```txt
- 코루틴 시작/종료
- 발사 루프
- 타겟 검색
- 쿨타임 처리
- 데미지 호출
- C++ 탱크 API 호출
```

쿨타임, 타겟, 현재 타겟, 슬롯 상태 같은 런타임 상태를 `ATankActor`에 넣지 않는다.

---

### 새 무기 등록

새 무기를 추가하면 `WeaponInventory.lua`의 생성자 테이블과 선택 가능 ID 목록에 등록한다.

```txt
Asset/Scripts/WeaponInventory.lua
```

현재 구조:

```lua
local WeaponConstructors = {
    MainCannon = require("Weapons.MainCannonWeapon"),
    MachineTurret = require("Weapons.MachineTurretWeapon"),
    Aura = require("Weapons.AuraWeapon"),
    HomingMissile = require("Weapons.HomingMissileWeapon"),
    SatelliteBeam = require("Weapons.SatelliteBeamWeapon"),
}

local WeaponIds = {
    "MainCannon",
    "MachineTurret",
    "Aura",
    "HomingMissile",
    "SatelliteBeam",
}
```

새 무기를 선택지에 나오게 하려면 두 군데 모두 추가해야 한다.

---

## 2. 현재 공격 타입

현재 `WeaponDefs.lua` 기준 공격 타입은 다음과 같다.

```txt
MainCannon     -> LinearProjectile
MachineTurret  -> InstantTargetDamage
Aura           -> AreaTickDamage
HomingMissile  -> HomingMissile
SatelliteBeam  -> InstantTargetDamage
```

공격 타입별 처리 방식은 다르다. 발사체처럼 보이는 무기만 C++에서 `AProjectileActor`를 생성한다.

---

## 3. 공격 타입별 구현 규칙

### LinearProjectile

대표 무기:

```txt
MainCannon
```

흐름:

```txt
MainCannonWeapon.lua
-> Owner.FireLinearProjectile("MainCannon", self.Data, 0)
-> ATankActor::FireLinearProjectile
-> AProjectileActor 생성/풀 획득
-> ProjectileScript.lua가 충돌 데미지 처리
```

`MainCannonWeapon.lua`는 발사 루프만 관리한다.

```lua
function MainCannonWeapon:FireLoop()
    while self.IsRunning do
        if self.Owner ~= nil and self.Owner.FireLinearProjectile ~= nil then
            self.Owner.FireLinearProjectile("MainCannon", self.Data, 0)
        else
            Log("[MainCannon] FireLinearProjectile is nil")
        end

        Co.Wait(self.Data.FireInterval)
    end
end
```

발사 위치는 `WeaponVisualDefs.lua`의 `Muzzle_MainCannon_0`에서 나온다.  
C++에서 `forward * 3.0f` 같은 추가 오프셋을 넣지 않는다.

---

### InstantTargetDamage

대표 무기:

```txt
MachineTurret
SatelliteBeam
```

즉발 데미지 무기는 발사체를 생성하지 않는다.  
실제 HP 감소는 Lua `DamageSystem.ApplyDamage()`에서 처리한다.

C++의 `NotifyInstantHit` 또는 `ApplyTargetDamage`는 데미지용이 아니라 이펙트/로그용 훅이다.  
C++에서 중복 데미지를 적용하면 안 된다.

#### MachineTurret

`MachineTurretWeapon.lua`는 포탑별 상태를 `TurretSlot`에 따로 가진다.

```txt
MachineTurretWeapon
 ├─ TurretSlots[0]
 │   ├─ Visual
 │   ├─ Muzzle
 │   ├─ Target
 │   ├─ FireTimer
 │   ├─ TargetTimer
 │   └─ Coroutine
 ├─ TurretSlots[1]
 ├─ TurretSlots[2]
 └─ TurretSlots[3]
```

포탑 수는 현재 다음 구조다.

```txt
Level 1 -> 1개
Level 2 -> 2개
Level 3 -> 4개
```

각 슬롯은 자기 타겟과 타이머를 따로 가진다.  
포탑 4개가 `self.Target` 하나를 공유하는 구조는 금지다.

흐름:

```txt
MachineTurretWeapon.lua
-> slot별 target 검색
-> slot.Visual:LookAt(targetPos)
-> DamageSystem.ApplyDamage(slot.Target, slot.Damage, ownerActor)
-> Owner.NotifyInstantHit("MachineTurret", self.Data, slot.Index, slot.Target)
```

`MachineTurret`은 `FireLinearProjectile`을 호출하면 안 된다.

#### SatelliteBeam

`SatelliteBeamWeapon.lua`는 가까운 Enemy를 찾아 즉시 데미지를 준다.

```txt
SatelliteBeamWeapon.lua
-> QueryActorByTagClosest("Enemy", ...)
-> DamageSystem.ApplyDamage(target, self.Data.Damage, ownerActor)
-> Owner.ApplyTargetDamage("SatelliteBeam", self.Data, target)
```

`ApplyTargetDamage`는 이펙트/로그용이다.

---

### HomingMissile

대표 무기:

```txt
HomingMissile
```

흐름:

```txt
HomingMissileWeapon.lua
-> target 검색
-> Owner.FireHomingMissile("HomingMissile", self.Data, 0, target)
-> ATankActor::FireHomingMissile
-> AHomingMissileActor 생성/풀 획득
-> AHomingMissileActor가 target 추적
-> Explode()
```

주의: 현재 `AHomingMissileActor::Explode()`는 실제 데미지 적용이 구현되어 있지 않고 warning 로그를 남긴다.

현재 상태:

```txt
미사일 추적/탄착 구조       있음
탄착점 실제 데미지 적용     미구현
```

나중에 구현할 때는 두 방식 중 하나를 선택해야 한다.

```txt
A안: C++에서 Enemy 검색/TakeDamage 처리
B안: Lua DamageSystem과 연결해서 ApplyDamage 호출
```

---

### AreaTickDamage

대표 무기:

```txt
Aura
```

현재 `AuraWeapon.lua`는 `Owner.ApplyAreaDamage(...)`를 호출한다.

```txt
AuraWeapon.lua
-> Owner.ApplyAreaDamage("Aura", self.Data, ownerActor:GetLocation(), self.Data.Radius)
```

주의: 현재 `ATankActor::ApplyAreaDamage()`는 실제 범위 데미지가 아니라 warning 로그가 있는 스텁이다.

현재 상태:

```txt
Aura 루프 구조             있음
C++ 범위 데미지 검색       미구현
```

팀원이 Aura를 완성하려면 `ApplyAreaDamage`를 구현하거나 Lua에서 반경 검색 후 `DamageSystem.ApplyDamage`를 호출해야 한다.

---

### Installable / Summon

현재 `SpawnInstallable`, `SpawnSummon`은 구조만 있는 스텁이다.

```txt
SpawnInstallable -> 지뢰 / 장판 / 포격 마커용 예정
SpawnSummon      -> 자동차 / 기차 소환형 공격용 예정
```

아직 실제 게임 로직은 없다. 호출되면 warning 로그를 남긴다.

---

## 4. 발사체 데미지 경로

직선 발사체 데미지는 `ProjectileScript.lua`가 담당한다.

```txt
Asset/Scripts/ProjectileScript.lua
```

C++ `AProjectileActor`는 다음 값을 저장한다.

```txt
Damage
PierceCount
```

그리고 `ProjectileScript.lua`에 다음 함수를 바인딩한다.

```txt
GetProjectileDamage()
ConsumePierce()
ReturnToPool()
```

`ProjectileScript.lua`가 담당하는 것:

```txt
OnProjectileFired
수명 종료 시 ReturnToPool
OnOverlapBegin
Enemy 태그 필터링
같은 적 중복 피격 방지
DamageSystem.ApplyDamage
ConsumePierce
관통 종료 시 ReturnToPool
```

런타임 발사체는 더 이상 `Asset/Scripts/Test/TestBullet.lua`를 사용하지 않는다.

---

## 5. 데미지 시스템

데미지 전달은 Lua 모듈이 담당한다.

```txt
Asset/Scripts/Core/DamageSystem.lua
```

`DamageSystem`은 UUID 기준으로 데미지를 받을 스크립트를 등록한다.

```lua
DamageSystem.Register(actorHandle, scriptInstance)
DamageSystem.Unregister(actorHandle)
DamageSystem.ApplyDamage(targetActor, amount, attacker)
```

적의 체력 처리는 다음 파일이 담당한다.

```txt
Asset/Scripts/Components/EnemyState.lua
```

`EnemyState`는 `BeginPlay`에서 DamageSystem에 자신을 등록한다.

```lua
DamageSystem.Register(self.actor, self)
```

데미지를 받으면:

```txt
EnemyState:TakeDamage
-> CurrentHP 감소
-> HP <= 0이면 Die
-> Gem spawn
-> PoolManager:Release(actor)
```

즉, 새 즉발 무기나 발사체는 직접 적 HP를 만지지 말고 `DamageSystem.ApplyDamage`를 사용한다.

---

## 6. C++ 책임 정리

### ATankActor

`ATankActor`가 담당하는 것:

```txt
- WeaponVisualDefs.lua layout table 읽기
- Visual / Muzzle 컴포넌트 생성 및 갱신
- Muzzle_<WeaponId>_<Index> 조회
- LinearProjectile 생성
- HomingMissile 생성
- InstantHit 이펙트/로그 훅
- 미구현 공격 타입 스텁 로그
```

`ATankActor`가 담당하면 안 되는 것:

```txt
- 레벨
- 경험치
- WeaponInventory 상태
- 무기 쿨타임
- 무기별 현재 타겟
- MachineTurret 슬롯 상태
- 레벨업 선택지
```

### AProjectileActor

`AProjectileActor`가 담당하는 것:

```txt
- Damage 저장
- PierceCount 저장
- ProjectileScript.lua에 함수 바인딩
- Fire 시 OnProjectileFired 호출
- MovementComponent에 초기 속도 전달
```

### AHomingMissileActor

`AHomingMissileActor`가 담당하는 것:

```txt
- TargetActor 추적
- TurnSpeed 기반 방향 보간
- 탄착 거리 도달 시 Explode 호출
```

현재 미구현:

```txt
- Explode 시 실제 데미지 적용
```

---

## 7. 새 무기 추가 체크리스트

새 무기를 추가할 때는 아래 순서대로 작업한다.

### 1단계: 수치 데이터 추가

```txt
Asset/Scripts/WeaponDefs.lua
```

예:

```lua
WeaponDefs.NewWeapon = {
    DisplayName = "New Weapon",
    AttackType = "LinearProjectile",
    MaxLevel = 3,
    Levels = {
        [1] = {
            Damage = 10,
            FireInterval = 1.0,
            Range = 10000.0,
        },
    },
}
```

### 2단계: 외형 배치 추가

```txt
Asset/Scripts/WeaponVisualDefs.lua
```

예:

```lua
WeaponVisualDefs.NewWeapon = {
    [1] = {
        Visuals = {
            {
                Name = "Visual_NewWeapon_0",
                Mesh = "Models/_Basic/Cube.OBJ",
                Parent = "RootComponent",
                Location = { 0.0, 0.0, 1.0 },
                Rotation = { 0.0, 0.0, 0.0 },
                Scale = { 1.0, 1.0, 1.0 },
                MuzzleName = "Muzzle_NewWeapon_0",
                MuzzleLocation = { 1.0, 0.0, 0.0 },
            },
        },
    },
}
```

### 3단계: 동작 파일 추가

```txt
Asset/Scripts/Weapons/NewWeaponWeapon.lua
```

여기에 코루틴, 타겟 검색, 공격 API 호출을 작성한다.

### 4단계: WeaponInventory에 등록

```txt
Asset/Scripts/WeaponInventory.lua
```

```lua
local WeaponConstructors = {
    NewWeapon = require("Weapons.NewWeaponWeapon"),
}

local WeaponIds = {
    "NewWeapon",
}
```

### 5단계: 공격 타입에 맞는 API 호출

```txt
LinearProjectile     -> Owner.FireLinearProjectile(...)
HomingMissile        -> Owner.FireHomingMissile(...)
InstantTargetDamage  -> DamageSystem.ApplyDamage(...) + Owner.NotifyInstantHit/ApplyTargetDamage(...)
AreaTickDamage       -> Owner.ApplyAreaDamage(...) 또는 Lua 반경 검색 + DamageSystem.ApplyDamage(...)
Installable          -> Owner.SpawnInstallable(...)
Summon               -> Owner.SpawnSummon(...)
```

---

## 8. 현재 남은 스텁 / 미구현 목록

현재 기준으로 의도적으로 스텁인 부분이다.

```txt
ATankActor::ApplyAreaDamage
- 현재 warning 로그만 출력
- Aura 실제 범위 데미지 완성 필요

ATankActor::SpawnInstallable
- 현재 warning 로그만 출력
- 지뢰/장판/포격 마커 구현 필요

ATankActor::SpawnSummon
- 현재 warning 로그만 출력
- 자동차/기차 소환형 공격 구현 필요

AHomingMissileActor::Explode
- 현재 warning 로그 후 풀 반환
- 실제 데미지 적용 미구현

ATankActor::NotifyInstantHit
- 현재 이펙트/로그 훅
- 실제 레이저/히트 이펙트 렌더링 미구현

ATankActor::ApplyTargetDamage
- 현재 이펙트/로그 훅
- 실제 데미지는 Lua DamageSystem에서 처리
```

스텁 함수가 조용히 실패하면 안 된다. 반드시 warning 또는 info 로그를 남겨야 한다.

---

## 9. 제거되었거나 금지된 레거시 구조

아래 구조는 다시 만들지 않는다.

```txt
ProjectileActor가 ProjectileScript.lua와 Test/TestBullet.lua를 동시에 붙이는 구조
Test/TestBullet.lua가 런타임 발사체 데미지를 처리하는 구조
TankScript.lua가 HeadMainGun 타겟팅/발사 루프를 직접 돌리는 구조
ATankActor가 Muzzle을 못 찾았을 때 HeadMainGun으로 fallback하는 구조
C++에서 Muzzle 위치에 forward * 3.0f 같은 하드코딩 오프셋을 추가하는 구조
MachineTurret이 FireLinearProjectile을 호출하는 구조
MachineTurret 포탑들이 하나의 target 상태를 공유하는 구조
```

`FireHeadMainGun`은 호환용 래퍼로만 남아 있다.

```txt
FireHeadMainGun(params)
-> FireLinearProjectile("MainCannon", params, 0)
```

이 함수는 `HeadMainGun` 컴포넌트를 직접 참조하면 안 된다.

---

## 10. 디버깅 체크리스트

### 포탄이 안 나갈 때

```txt
1. WeaponVisualDefs.lua에 Muzzle_MainCannon_0이 있는지 확인
2. ATankActor 로그에 "Muzzle not found"가 있는지 확인
3. MainCannonWeapon.lua가 FireLinearProjectile을 호출하는지 확인
4. AProjectileActor::Fire가 호출되는지 확인
5. ProjectileScript.lua의 OnProjectileFired 로그 확인
```

### 포탄은 나가는데 데미지가 안 들어갈 때

```txt
1. ProjectileScript.lua OnOverlapBegin 호출 여부 확인
2. 상대 Actor tag가 "Enemy"인지 확인
3. EnemyState.lua가 DamageSystem.Register를 호출했는지 확인
4. DamageSystem.ApplyDamage 로그 확인
5. ProjectileScript.lua가 GetProjectileDamage를 읽는지 확인
```

### MachineTurret이 안 때릴 때

```txt
1. WeaponDefs.lua의 MachineTurret.TurretCount 확인
2. WeaponVisualDefs.lua에 Visual_MachineTurret_i가 있는지 확인
3. MachineTurretWeapon.lua에서 TurretSlot 생성 로그 확인
4. slot.Target이 유효한지 확인
5. DamageSystem.ApplyDamage 성공 여부 확인
6. NotifyInstantHit 로그 확인
```

### 무기 외형이 안 붙을 때

```txt
1. WeaponInventory.lua의 ApplyWeaponVisual 호출 확인
2. WeaponVisualDefs.lua에 해당 WeaponId와 Level이 있는지 확인
3. Parent 이름이 RootComponent 등 실제 컴포넌트 이름과 일치하는지 확인
4. Mesh 경로가 올바른지 확인
5. ATankActor::EquipWeaponVisualFromLua 로그 확인
```

---

## 11. 팀 작업 규칙

무기를 수정할 때는 다음 원칙을 지킨다.

```txt
수치만 바꾼다          -> WeaponDefs.lua
배치만 바꾼다          -> WeaponVisualDefs.lua
동작만 바꾼다          -> Weapons/<WeaponName>Weapon.lua
선택 가능 목록을 바꾼다 -> WeaponInventory.lua
발사체 런타임을 바꾼다  -> ProjectileScript.lua + AProjectileActor
적 체력을 바꾼다        -> EnemyState.lua
데미지 전달을 바꾼다    -> Core/DamageSystem.lua
```

`ATankActor`에 새 무기 전용 레벨, 쿨타임, 타겟 상태를 추가하지 않는다.  
무기별 상태는 무기 Lua 객체 또는 해당 무기의 슬롯 객체가 가져야 한다.

