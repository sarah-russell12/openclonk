/*
	Clonk
	Author: Randrian

	The protoganist of the game. Witty and nimble if skillfully controlled ;-)
*/


// selectable by HUD and some performance optimizations for the HUD updates
#include Library_HUDAdapter
// standard controls
#include Library_ClonkControl
#include Library_CarryHeavyControl
// manager for aiming
#include Library_AimManager

#include Clonk_Animations

// un-comment them as soon as the new controls work with context menus etc.^
// Context menu
//#include Library_ContextMenu
// Auto production
//#include Library_AutoProduction

// ladder climbing
#include Library_CanClimbLadder

/* Initialization */

protected func Construction()
{
	_inherited(...);
	
	SetSkin(0);

	AddEffect("IntTurn", this, 1, 1, this);
	AddEffect("IntEyes", this, 1, 35+Random(4), this);

	AttachBackpack();
	iHandMesh = [0,0];
	
	SetAction("Walk");
	SetDir(Random(2));
	// Broadcast for rules
	GameCallEx("OnClonkCreation", this);
}

/* When adding to the crew of a player */

protected func Recruitment(int iPlr)
{
	//The clonk's appearance
	//Player settings can be overwritten for individual Clonks. In your clonk file: "ExtraData=1;Skin=iX" (X = chosen skin)
	var skin = GetCrewExtraData("Skin");
	if (skin == nil) skin = GetPlrClonkSkin(iPlr);
	if(skin != nil) SetSkin(skin);
	else SetSkin(Random(GetSkinCount()));

	// Broadcast for crew
	GameCallEx("OnClonkRecruitment", this, iPlr);
	
	return _inherited(iPlr,...);
}

protected func DeRecruitment(int iPlr) {
	// Broadcast for crew
	GameCallEx("OnClonkDeRecruitment", this, iPlr);
	
	return _inherited(iPlr,...);
}


protected func ControlCommand(szCommand, pTarget, iTx, iTy, pTarget2, Data)
{
	// RejectConstruction Callback for building via Drag'n'Drop form a building menu
	// TODO: Check if we still need this with the new system
	if(szCommand == "Construct")
	{
		if(Data->~RejectConstruction(iTx - GetX(), iTy - GetY(), this) )
		{
			return 1;
		}
	}
	// No overloaded command
	return _inherited(szCommand, pTarget, iTx, iTy, pTarget2, Data, ...);
}


/* Transformation */

public func Redefine(idTo)
{
	// save data of activity
	var phs=GetPhase(),act=GetAction();
	// Transform
	ChangeDef(idTo);
	// restore action
	var chg=SetAction(act);
	if (!chg) SetAction("Walk");
	if (chg) SetPhase(phs);
	// Done
	return 1;
}

/* Events */

protected func CatchBlow()
{
	if (GetAction() == "Dead") return;
	if (!Random(5)) PlaySoundHurt();
}
	
protected func Grab(object pTarget, bool fGrab)
{
	if (fGrab)
		Sound("Clonk::Action::Grab");
	else
		Sound("Clonk::Action::UnGrab");
}

protected func Get()
{
	Sound("Clonk::Action::Grab");
}

protected func Put()
{
	Sound("Clonk::Action::Grab");
}

protected func Death(int killed_by)
{
	// this must be done first, before any goals do funny stuff with the clonk
	_inherited(killed_by,...);
	
	// Info-broadcasts for dying clonks.
	GameCallEx("OnClonkDeath", this, killed_by);
	
	// The broadcast could have revived the clonk.
	if (GetAlive())
		return;
	
	// Some effects on dying.
	if (!this.silent_death)
	{
		PlaySkinSound("Die*");
		DeathAnnounce();
		
		// When killed by a team member, the other Clonk randomly plays a sound.
		if (!Random(5) && killed_by != NO_OWNER && killed_by != GetOwner() && !Hostile(killed_by, GetOwner()))
		{
			var other_cursor = GetCursor(killed_by);
			if (other_cursor)
				other_cursor->~PlaySoundTaunt();
		}
	}
	CloseEyes(1);
	
	return true;
}

protected func Destruction(...)
{
	_inherited(...);
	// If the clonk wasn't dead yet, he will be now.
	// Always kill clonks first. This will ensure relaunch scripts, enemy kill counters, etc. are called
	// even if clonks die in some weird way that causes direct removal
	// (To prevent a death callback, you can use SetAlive(false); RemoveObject();)
	if (GetAlive()) { this.silent_death=true; Kill(); }
	return true;
}

protected func DeepBreath()
{
	Sound("Breath");
}

public func Incineration()
{
	PlaySoundShock();
	return _inherited(...);
}

protected func CheckStuck()
{
	// Prevents getting stuck on middle vertex
	if(!GetXDir()) if(Abs(GetYDir()) < 5)
		if(GBackSolid(0, 3))
			SetPosition(GetX(), GetY() + 1);
}

public func Eat(object food)
{
	if(GetProcedure() == "WALK")
	{
		Heal(food->NutritionalValue());
		food->RemoveObject();
		Sound("Clonk::Action::Munch?");
		SetAction("Eat");
	}
}

// Called when an object was dug free.
func DigOutObject(object obj)
{
	// Some materials can only be transported/collected with a bucket.
	if (obj->~IsBucketMaterial())
	{
		// Assume we might already be carrying a filled bucket and the object is stackable, try it!
		var handled = obj->~TryPutInto(this);
		if (!handled)
		{
			// Otherwise, force into empty buckets!
			var empty_bucket = FindObject(Find_Container(this), Find_Func("IsBucket"), Find_Func("IsBucketEmpty"));
			if (empty_bucket)
			{
				obj->Enter(empty_bucket);
				handled = true;
			}
		}
		// Those objects can only be carried with a bucket, sadly...
		if (!handled)
			obj->RemoveObject();
		// Object might have been removed now.
		return;
	}
	// Collect fragile objects when dug out
	if (obj->GetDefFragile())
		return Collect(obj,nil,nil,true);
	return false;
}

// Building material bridges (like loam bridge)
func Bridge()
{
	var proc = GetProcedure();
	// Clonk must stand on ground. Allow during SCALE; but Clonk won't keep animation if he's not actually near the ground
	if (proc != "WALK" && proc != "SCALE")
		return false;
	if (proc == "WALK")
		SetAction("BridgeStand");
	else
		SetAction("BridgeScale");
	SetComDir(COMD_Stop);
	SetXDir(0);
	SetYDir(0);
	return true;
}

/* Status */

// TODO: Make this more sophisticated, readd turn animation and other
// adaptions
public func IsClonk() { return true; }
public func IsPrey() { return true; }

public func IsJumping(){return WildcardMatch(GetAction(), "*Jump*");}
public func IsWalking(){return GetProcedure() == "WALK";}
public func IsSwimming(){return GetProcedure() == "SWIM";}
public func IsBridging(){return WildcardMatch(GetAction(), "Bridge*");}

// Clonks act as containers for the interaction menu as long as they are alive.
public func IsContainer() { return GetAlive(); }

// You can not interact with dead Clonks.
// This would be the place to show a death message etc.
public func RejectInteractionMenu(object to)
{
	if (!GetAlive())
		return Format("$MsgDeadClonk$", GetName());
	return _inherited(to, ...);
}

public func GetSurroundingEntryMessage(object for_clonk)
{
	if (!GetAlive()) return Format("{{Clonk_Grave}} %s", Clonk_Grave->GetInscriptionForClonk(this));
}

/* Carry items on the clonk */

local iHandMesh;
local fHandAction;
local fBothHanded;

func OnSelectionChanged(int oldslot, int newslot, bool secondaryslot)
{
	AttachHandItem(secondaryslot);
	return _inherited(oldslot, newslot, secondaryslot);
}
func OnSlotEmpty(int slot)
{
	AttachHandItem(slot);
	return _inherited(slot);
}
func OnSlotFull(int slot)
{
	AttachHandItem(slot);
	return _inherited(slot);
}

public func DetachObject(object obj)
{
	if(GetHandItem(0) == obj)
		DetachHandItem(0);
	if(GetHandItem(1) == obj)
		DetachHandItem(1);
}

func DetachHandItem(bool secondary)
{
	if(iHandMesh[secondary])
		DetachMesh(iHandMesh[secondary]);
	iHandMesh[secondary] = 0;
}

func AttachHandItem(bool secondary)
{
	DetachHandItem(secondary);
	UpdateAttach();
}

func UpdateAttach()
{
	StopAnimation(GetRootAnimation(6));
	DoUpdateAttach(0);
	DoUpdateAttach(1);
}

func DoUpdateAttach(bool sec)
{
	var obj = GetHandItem(sec);
	var other_obj = GetHandItem(!sec);
	if(!obj) return;
	var iAttachMode = obj->~GetCarryMode(this);
	if(iAttachMode == CARRY_None) return;

	if(iHandMesh[sec])
	{
		DetachMesh(iHandMesh[sec]);
		iHandMesh[sec] = 0;
	}

	var bone = "main";
	var bone2;
	if(obj->~GetCarryBone())  bone  = obj->~GetCarryBone(this);
	if(obj->~GetCarryBone2()) bone2 = obj->~GetCarryBone2(this);
	else bone2 = bone;
	var nohand = 0;
	if(!HasHandAction(sec, 1)) nohand = 1;
	var trans = obj->~GetCarryTransform(this, sec, nohand);

	var pos_hand = "pos_hand2";
	if(sec) pos_hand = "pos_hand1";
	var pos_back = "pos_back1";
	if(sec) pos_back = "pos_back2";
	var closehand = "Close2Hand";
	if(sec) closehand = "Close1Hand";

	if(!sec) fBothHanded = 0;

	var special = obj->~GetCarrySpecial(this);
	var special_other;
	if(other_obj) special_other = other_obj->~GetCarrySpecial(this);
	if(special)
	{
		iHandMesh[sec] = AttachMesh(obj, special, bone, trans);
		iAttachMode = 0;
	}

	if(iAttachMode == CARRY_Hand)
	{
		if(HasHandAction(sec, 1))
		{
			iHandMesh[sec] = AttachMesh(obj, pos_hand, bone, trans);
			PlayAnimation(closehand, CLONK_ANIM_SLOT_Hands, Anim_Const(GetAnimationLength(closehand)), Anim_Const(1000));
		}
	}
	else if(iAttachMode == CARRY_HandBack)
	{
		if(HasHandAction(sec, 1))
		{
			iHandMesh[sec] = AttachMesh(obj, pos_hand, bone, trans);
			PlayAnimation(closehand, CLONK_ANIM_SLOT_Hands, Anim_Const(GetAnimationLength(closehand)), Anim_Const(1000));
		}
		else
			iHandMesh[sec] = AttachMesh(obj, pos_back, bone2, trans);
	}
	else if(iAttachMode == CARRY_HandAlways)
	{
		iHandMesh[sec] = AttachMesh(obj, pos_hand, bone, trans);
		PlayAnimation(closehand, CLONK_ANIM_SLOT_Hands, Anim_Const(GetAnimationLength(closehand)), Anim_Const(1000));
	}
	else if(iAttachMode == CARRY_Back)
	{
		iHandMesh[sec] = AttachMesh(obj, pos_back, bone2, trans);
	}
	else if(iAttachMode == CARRY_BothHands)
	{
		if(sec) return;
		if(HasHandAction(sec, 1) && !sec && !special_other)
		{
			iHandMesh[sec] = AttachMesh(obj, "pos_tool1", bone, trans);
			PlayAnimation("CarryArms", CLONK_ANIM_SLOT_Hands, Anim_Const(obj->~GetCarryPhase(this)), Anim_Const(1000));
			fBothHanded = 1;
		}
	}
	else if(iAttachMode == CARRY_Spear)
	{
		if(HasHandAction(sec, 1) && !sec)
		{
			PlayAnimation("CarrySpear", CLONK_ANIM_SLOT_Hands, Anim_Const(0), Anim_Const(1000));
		}
		else
			iHandMesh[sec] = AttachMesh(obj, pos_back, bone2, trans);
	}
	else if(iAttachMode == CARRY_Musket)
	{
		if(HasHandAction(sec, 1) && !sec)
		{
			iHandMesh[sec] = AttachMesh(obj, "pos_hand2", bone, trans);
			PlayAnimation("CarryMusket", CLONK_ANIM_SLOT_Hands, Anim_Const(0), Anim_Const(1000));
			fBothHanded = 1;
		}
		else
			iHandMesh[sec] = AttachMesh(obj, pos_back, bone2, trans);
	}
	else if(iAttachMode == CARRY_Grappler)
	{
		if(HasHandAction(sec, 1) && !sec)
		{
			iHandMesh[sec] = AttachMesh(obj, "pos_hand2", bone, trans);
			PlayAnimation("CarryCrossbow", CLONK_ANIM_SLOT_Hands, Anim_Const(0), Anim_Const(1000));
			fBothHanded = 1;
		}
		else
			iHandMesh[sec] = AttachMesh(obj, pos_back, bone2, trans);
	}
}//AttachMesh(DynamiteBox, "pos_tool1", "main", Trans_Translate(0,0,0));

public func GetHandMesh(object obj)
{
	if(GetHandItem(0) == obj)
		return iHandMesh[0];
	if(GetHandItem(1) == obj)
		return iHandMesh[1];
}

static const CARRY_None         = 0;
static const CARRY_Hand         = 1;
static const CARRY_HandBack     = 2;
static const CARRY_HandAlways   = 3;
static const CARRY_Back         = 4;
static const CARRY_BothHands    = 5;
static const CARRY_Spear        = 6;
static const CARRY_Musket       = 7;
static const CARRY_Grappler     = 8;

func HasHandAction(sec, just_wear, bool force_landscape_letgo)
{
	// Check if the clonk is currently able to use hands
	// sec: Needs both hands (e.g. CarryHeavy?)
	// just_wear: ???
	// force_landscape_letgo: Also allow from actions where hands are currently grabbing the landscape (scale, hangle)
	if(sec && fBothHanded)
		return false;
	if(just_wear)
	{
		if( HasActionProcedure(force_landscape_letgo) && !fHandAction ) // For wear purpose fHandAction==-1 also blocks
			return true;
	}
	else
	{
		if( HasActionProcedure(force_landscape_letgo) && (!fHandAction || fHandAction == -1) )
			return true;
	}
	return false;
}

func HasActionProcedure(bool force_landscape_letgo)
{
	// Check if the clonk is currently in an action where he could use his hands
	// if force_landscape_letgo is true, also allow during scale/hangle assuming the clonk will let go
	var action = GetAction();
	if (action == "Walk" || action == "Jump" || action == "WallJump" || action == "Kneel" || action == "Ride" || action == "BridgeStand")
		return true;
	if (force_landscape_letgo) if (action == "Scale" || action == "Hangle")
		return true;
	return false;
}

public func ReadyToAction(fNoArmCheck)
{
	if(!fNoArmCheck)
		return HasActionProcedure();
	return HasHandAction(0);
}

public func SetHandAction(bool fNewValue)
{
	if(fNewValue > 0)
		fHandAction = 1; // 1 means can't use items and doesn't draw items in hand
	else if(fNewValue < 0)
		fHandAction = -1; // just don't draw items in hand can still use them
	else
		fHandAction = 0;
	UpdateAttach();
}

public func GetHandAction()
{
	if(fHandAction == 1)
		return true;
	return false;
}

/* Enable the Clonk to pick up stuff from its surrounding in the interaction menu */
public func OnInteractionMenuOpen(object menu)
{
	_inherited(menu, ...);
	
	// Allow picking up stuff from the surrounding only if not in a container itself.
	if (!Contained())
	{
		var surrounding = CreateObject(Helper_Surrounding);
		surrounding->InitFor(this, menu);
	}
}

/* Mesh transformations */

local mesh_transformation_list;

func SetMeshTransformation(array transformation, int layer)
{
	if(!mesh_transformation_list) mesh_transformation_list = [];
	if(GetLength(mesh_transformation_list) < layer)
		SetLength(mesh_transformation_list, layer+1);
	mesh_transformation_list[layer] = transformation;
	var all_transformations = nil;
	for(var trans in mesh_transformation_list)
	{
		if(!trans) continue;
		if(all_transformations)
			all_transformations = Trans_Mul(trans, all_transformations);
		else
			all_transformations = trans;
	}
	SetProperty("MeshTransformation", all_transformations);
}

/* Backpack */

local backpack;

func AttachBackpack()
{
	//Places a backpack onto the clonk
	backpack = AttachMesh(BackpackGraphic, "skeleton_body", "main",       
	                      Trans_Mul(Trans_Rotate(180,1,0,0), Trans_Scale(700,400,700), Trans_Translate(4000,-1000,0)));
}

func RemoveBackpack()
{
	if(backpack)
	{
		DetachMesh(backpack);
		backpack = nil;
	}
	else return false;
}

// calback from engine
func OnMaterialChanged(int new, int old)
{
	if(!GetAlive()) return;
	var newdens = GetMaterialVal("Density","Material",new);
	var olddens = GetMaterialVal("Density","Material",old);
	var newliquid = (newdens >= C4M_Liquid) && (newdens < C4M_Solid);
	var oldliquid = (olddens >= C4M_Liquid) && (olddens < C4M_Solid);
	// into water
	if(newliquid && !oldliquid)
		AddEffect("Bubble", this, 1, 8, this);
	// out of water
	else if(!newliquid && oldliquid)
		RemoveEffect("Bubble", this);
}

func FxBubbleTimer(pTarget, effect, iTime)
{
	if(GBackLiquid(0,-5))
	{
		var mouth_off = GetCon()/11;
		var iRot = GetSwimRotation();
		var mouth_off_x = Sin(iRot, mouth_off), mouth_off_y = Cos(iRot, mouth_off);
		// Search for bubbles to breath from
		var bubble = FindObject(Find_Func("CanBeBreathed", this), Find_AtRect(mouth_off_x-mouth_off/2, mouth_off_y, mouth_off, mouth_off/3));
		if (bubble)
		{
			bubble->~OnClonkBreath(this);
		}
		else if (!Random(6))
		{
			// Make your own bubbles
			
			Bubble(1, mouth_off_x, mouth_off_y);
		}
	}
}

func QueryCatchBlow(object obj)
{
	var r=0;
	var e=0;
	var i=0;
	// Blocked by object effects?
	while(e=GetEffect("*", obj, i++))
		if(EffectCall(obj, e, "QueryHitClonk", this))
			return true;
	// Blocked by Clonk effects?
	i=0;
	while(e=GetEffect("*Control*", this, i++))
	{
		if(EffectCall(this, e, "QueryCatchBlow", obj))
		{
			r=true;
			break;
		}
		
	}
	if(r) return r;
	// No blocking
	return _inherited(obj, ...);
}

local gender, skin, skin_name;

func SetSkin(int new_skin)
{
	// Remember skin
	skin = new_skin;
	
	//Adventurer
	if(skin == 0)
	{	SetGraphics(skin_name = nil);
		gender = 0;	}

	//Steampunk
	if(skin == 1)
	{	SetGraphics(skin_name = "Steampunk");
		gender = 1; }

	//Alchemist
	if(skin == 2)
	{	SetGraphics(skin_name = "Alchemist");
		gender = 0;	}
	
	//Farmer
	if(skin == 3)
	{	SetGraphics(skin_name = "Farmer");
		gender = 1;	}

	RemoveBackpack(); //add a backpack
	AttachBackpack();
	//refreshes animation (whatever that means?)
	// Go back to original action afterwards and hope
	// that noone calls SetSkin during more compex activities
	var prev_action = GetAction();
	SetAction("Jump");
	SetAction(prev_action);

	return skin;
}
func GetSkinCount() { return 4; }

func GetSkin() { return skin; }
func GetSkinName() { return skin_name; }


// Returns the skin name as used to select the right sound subfolder.
public func GetSoundSkinName()
{
	if (skin_name == nil) return "Adventurer";
	return skin_name;
}

public func PlaySkinSound(string sound, ...)
{
	Sound(Format("Clonk::Skin::%s::%s", GetSoundSkinName(), sound), ...);
}

/*
Helper functions to play some sounds. This are encapsulated here in case sound names change.
*/
public func PlaySoundConfirm(...)
{
	if (GetSoundSkinName() != "Farmer")
		PlaySkinSound("Confirm*", ...);
}
public func PlaySoundDecline(...)
{
	if (GetSoundSkinName() != "Farmer")
		PlaySkinSound("Decline*", ...);
}
// Doubtful sound, e.g. when trying a clearly impossible action.
public func PlaySoundDoubt(...)
{
	if (GetSoundSkinName() != "Farmer")
		PlaySkinSound("Doubt*", ...);
}

public func PlaySoundHurt(...) { PlaySkinSound("Hurt*", ...); }
// Sound that is supposed to be funny in situations where the Clonk maybe did something "evil" like killing a teammate.
public func PlaySoundTaunt(...)
{
	if (GetSoundSkinName() == "Alchemist")
		PlaySkinSound("EvilConfirm*", ...);
	else if (GetSoundSkinName() == "Steampunk")
		PlaySkinSound("Laughter*", ...);
}
// Surprised sounds, e.g. when catching fire.
public func PlaySoundShock(...)
{
	if (GetSoundSkinName() == "Steampunk" || GetSoundSkinName() == "Adventurer")
		PlaySkinSound("Shock*", ...);
}
public func PlaySoundScream() { PlaySkinSound("Scream*"); }
// General idle sounds, played when also playing an idle animation.
public func PlaySoundIdle(...)
{
	if (GetSoundSkinName() == "Steampunk")
		PlaySkinSound("Singing*", ...);
}
//Portrait definition of this Clonk for messages
func GetPortrait()
{
	return this.portrait ?? { Source = GetID(), Name = Format("Portrait%s", skin_name ?? ""), Color = GetColor() };
}

func SetPortrait(proplist custom_portrait)
{
	this.portrait = custom_portrait;
	return true;
}

public func CommandFailure() { return PlaySoundDoubt(); } // Callback from the engine when a command failed

/* Magic */

local magic_energy;

public func GetMagicEnergy(int precision)
{
	if (precision == nil) precision = 1000;

	if (precision)
		return magic_energy / precision;
	else
		return magic_energy;
}

public func GetMaxMagicEnergy(int precision)
{
	if (precision == nil) precision = 1000;

	if (precision)
		return this.MaxMagic / precision;
	else
		return this.MaxMagic;
}

public func SetMagicEnergy(int val, int precision)
{
	if (precision == nil) precision = 1000;

	magic_energy = BoundBy(val * precision, 0, this.MaxMagic);
	this->~OnMagicEnergyChange(val);

	return true;
}

// Adjusts the magic energy but only if change can be applied completely. Returns true if successful, false otherwise.
// Use partial to bypass the completeness check
public func DoMagicEnergy(int change, bool partial, int precision)
{
	if (precision == nil) precision = 1000;
	change = change * precision;

	// Can't apply fully?
	if (!Inside(magic_energy + change, 0, this.MaxMagic) && !partial)
		return false;

	magic_energy = BoundBy(magic_energy + change, 0, this.MaxMagic);
	this->~OnMagicEnergyChange(change);
	return true;
}

/* Scenario saving */

func SaveScenarioObject(props)
{
	if (!inherited(props, ...)) return false;
	// Skins override mesh material
	if (skin)
	{
		props->Remove("MeshMaterial");
		props->AddCall("Skin", this, "SetSkin", skin);
	}
	// Direction is randomized at creation and there's no good way to find
	// out if the user wanted that specific direction. So just always save
	// it, because that's what scenario designer usually wants.
	if (!props->HasProp("Dir")) props->AddCall("Dir", this, "SetDir", GetConstantNameByValueSafe(GetDir(),"DIR_"));
	// Custom portraits for dialogues
	if (this.portrait) props->AddCall("Portrait", this, "SetPortrait", this.portrait);
	return true;
}

/* AI editor helper */

func EditCursorSelection(...)
{
	var ai = AI->GetAI(this);
	if (ai) Call(AI.EditCursorSelection, ai, ...);
	return _inherited(...);
}

func EditCursorDeselection(...)
{
	var ai = AI->GetAI(this);
	if (ai) Call(AI.EditCursorDeselection, ai, ...);
	return _inherited(...);
}

func AI_Add()
{
	// Create AI and re-select
	AI->AddAI(this);
	EditCursorDeselection();
	EditCursorSelection();
	return true;
}

func FlipDir()
{
	// Look the other way. If an AI is attached, also update its home position.
	var new_dir = 1-GetDir();
	if (this.ai) this.ai.home_dir = new_dir;
	return SetDir(new_dir);
}

local EditCursorCommands = ["AI_Add()", "FlipDir()"];

/* Act Map */

local ActMap = {
Walk = {
	Prototype = Action,
	Name = "Walk",
	Procedure = DFA_WALK,
	Accel = 16,
	Decel = 48,
	Speed = 200,
	Directions = 2,
	FlipDir = 0,
	Length = 1,
	Delay = 0,
	X = 0,
	Y = 0,
	Wdt = 8,
	Hgt = 20,
	StartCall = "StartWalk",
	AbortCall = "StopWalk",
//	InLiquidAction = "Swim",
},
Stand = {
	Prototype = Action,
	Name = "Stand",
	Procedure = DFA_THROW,
	Directions = 2,
	FlipDir = 0,
	Length = 1,
	Delay = 0,
	X = 0,
	Y = 0,
	Wdt = 8,
	Hgt = 20,
	StartCall = "StartStand",
	InLiquidAction = "Swim",
},
Kneel = {
	Prototype = Action,
	Name = "Kneel",
	Procedure = DFA_KNEEL,
	Directions = 2,
	FlipDir = 0,
	Length = 1,
	Delay = 0,
	X = 0,
	Y = 0,
	Wdt = 8,
	Hgt = 20,
//	StartCall = "StartKneel",
	InLiquidAction = "Swim",
},
Roll = {
	Prototype = Action,
	Name = "Roll",
	Procedure = DFA_NONE,
	Directions = 2,
	FlipDir = 0,
	Length = 1,
	Delay = 0,
	X = 0,
	Y = 0,
	Wdt = 8,
	Hgt = 20,
	StartCall = "OnStartRoll",
	AbortCall = "OnAbortRoll",
	NextAction = "Walk",
	InLiquidAction = "Swim",
},
Scale = {
	Prototype = Action,
	Name = "Scale",
	Procedure = DFA_SCALE,
	Speed = 60,
	Accel = 20,
	Directions = 2,
	Length = 1,
	Delay = 0,
	X = 0,
	Y = 20,
	Wdt = 8,
	Hgt = 20,
	OffX = 0,
	OffY = 0,
	StartCall = "StartScale",
	AbortCall = "StopScale",
},
Tumble = {
	Prototype = Action,
	Name = "Tumble",
	Procedure = DFA_FLIGHT,
	Speed = 200,
	Accel = 0,
	Directions = 2,
	Length = 1,
	Delay = 0,
	X = 0,
	Y = 40,
	Wdt = 8,
	Hgt = 20,
	NextAction = "Tumble",
	ObjectDisabled = 1,
	InLiquidAction = "Swim",
	StartCall = "StartTumble",
	AbortCall = "StopTumble",
	EndCall = "CheckStuck",
},
Dig = {
	Prototype = Action,
	Name = "Dig",
	Procedure = DFA_DIG,
	Speed = 50,
	Directions = 2,
	Length = 16,
	Delay = 0,//15*3*0,
	X = 0,
	Y = 60,
	Wdt = 8,
	Hgt = 20,
	NextAction = "Dig",
	StartCall = "StartDigging",
	AbortCall = "StopDigging",
	DigFree = 11,
//	InLiquidAction = "Swim",
	Attach = CNAT_Left | CNAT_Right | CNAT_Bottom,
},
BridgeStand = {
	Prototype = Action,
	Name = "BridgeStand",
	Procedure = DFA_THROW,
	Directions = 2,
	Length = 16,
	Delay = 1,
	X = 0,
	Y = 60,
	Wdt = 8,
	Hgt = 20,
	NextAction = "BridgeStand",
	StartCall = "StartStand",
	InLiquidAction = "Swim",
},
BridgeScale = {
	Prototype = Action,
	Name = "BridgeScale",
	Procedure = DFA_THROW,
	Directions = 2,
	Length = 16,
	Delay = 1,
	X = 0,
	Y = 60,
	Wdt = 8,
	Hgt = 20,
	NextAction = "BridgeScale",
	InLiquidAction = "Swim",
},
Swim = {
	Prototype = Action,
	Name = "Swim",
	Procedure = DFA_SWIM,
	Speed = 96,
	Accel = 7,
	Decel = 9,
	Directions = 2,
	Length = 1,
	Delay = 0,
	X = 0,
	Y = 80,
	Wdt = 8,
	Hgt = 20,
	OffX = 0,
	OffY = 0,
//	SwimOffset = -5,
	StartCall = "StartSwim",
	AbortCall = "StopSwim",
	Sound = "Clonk::Movement::DivingLoop*",
},
Hangle = {
	Prototype = Action,
	Name = "Hangle",
	Procedure = DFA_HANGLE,
	Speed = 48,
	Accel = 20,
	Directions = 2,
	Length = 1,
	Delay = 0,
	X = 0,
	Y = 100,
	Wdt = 8,
	Hgt = 20,
	OffX = 0,
	OffY = 3,
	StartCall = "StartHangle",
	AbortCall = "StopHangle",
	InLiquidAction = "Swim",
},
Jump = {
	Prototype = Action,
	Name = "Jump",
	Procedure = DFA_FLIGHT,
	Speed = 200,
	Accel = 14,
	Directions = 2,
	Length = 1,
	Delay = 0,
	X = 0,
	Y = 120,
	Wdt = 8,
	Hgt = 20,
	InLiquidAction = "Swim",
	PhaseCall = "CheckStuck",
	StartCall = "StartJump",
},
WallJump = {
	Prototype = Action,
	Name = "WallJump",
	Procedure = DFA_FLIGHT,
	Speed = 200,
	Accel = 14,
	Directions = 2,
	Length = 1,
	Delay = 0,
	X = 0,
	Y = 120,
	Wdt = 8,
	Hgt = 20,
	InLiquidAction = "Swim",
	PhaseCall = "CheckStuck",
},
Dive = {
	Prototype = Action,
	Name = "Dive",
	Procedure = DFA_FLIGHT,
	Speed = 200,
	Accel = 16,
	Directions = 2,
	Length = 8,
	Delay = 4,
	X = 0,
	Y = 160,
	Wdt = 8,
	Hgt = 20,
	NextAction = "Hold",
	ObjectDisabled = 1,
	InLiquidAction = "Swim",
	PhaseCall = "CheckStuck",
},
Dead = {
	Prototype = Action,
	Name = "Dead",
	Directions = 2,
	X = 0,
	Y = 240,
	Wdt = 8,
	Hgt = 20,
	Length = 1,
	Delay = 0,
	NextAction = "Hold",
	StartCall = "StartDead",
	NoOtherAction = 1,
	ObjectDisabled = 1,
},
Ride = {
	Prototype = Action,
	Name = "Ride",
	Procedure = DFA_ATTACH,
	Directions = 2,
	Length = 1,
	Delay = 0,
	X = 128,
	Y = 120,
	Wdt = 8,
	Hgt = 20,
	StartCall = "StartRiding",
	AbortCall = "StopRiding",
	InLiquidAction = "Swim",
},
Push = {
	Prototype = Action,
	Name = "Push",
	Procedure = DFA_PUSH,
	Speed = 200,
	Accel = 100,
	Directions = 2,
	Length = 8,
	Delay = 15,
	X = 128,
	Y = 140,
	Wdt = 8,
	Hgt = 20,
	NextAction = "Push",
	StartCall = "StartPushing",
	AbortCall = "StopPushing",
	InLiquidAction = "Swim",
},
Build = {
	Prototype = Action,
	Name = "Build",
	Directions = 2,
	Length = 8,
	Delay = 15,
	X = 128,
	Y = 140,
	Wdt = 8,
	Hgt = 20,
	NextAction = "Build",
	InLiquidAction = "Swim",
	Attach = CNAT_Bottom,
},
HangOnto = {
	Prototype = Action,
	Name = "HangOnto",
	Procedure = DFA_ATTACH,
	Directions = 2,
	Length = 1,
	Delay = 0,
	X = 128,
	Y = 120,
	Wdt = 8,
	Hgt = 20,
	StartCall = "StartHangOnto",
	AbortCall = "AbortHangOnto",
	InLiquidAction = "Swim",
},
Eat = {
	Prototype = Action,
	Name = "Eat",
	Procedure = DFA_NONE,
	Directions = 2,
	Length = 1,
	Delay = 45,
	X = 0,
	Y = 0,
	Wdt = 8,
	Hgt = 20,
	StartCall = "StartEat",
	NextAction = "Walk",
	InLiquidAction = "Swim",
	Attach=CNAT_Bottom,
},
};

local Name = "Clonk";
local Description = "$Description$";
local MaxEnergy = 50000;
local MaxBreath = 720; // Clonk can breathe for 20 seconds under water.
local MaxMagic = 50000;
local JumpSpeed = 400;
local ThrowSpeed = 294;
local NoBurnDecay = 1;
local ContactIncinerate = 10;
local BorderBound = C4D_Border_Sides;

func Definition(def) {
	// Set perspective
	SetProperty("PictureTransformation", Trans_Mul(Trans_Translate(0,1000,5000), Trans_Rotate(70,0,1,0)), def);

	_inherited(def);
}
