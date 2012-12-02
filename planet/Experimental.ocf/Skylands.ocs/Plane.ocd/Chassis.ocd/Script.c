/*--
	Plane part: Chassis
	Author: Sven2

	Used to construct the plane
--*/

private func Hit()
{
	Sound("WoodHit");
}

public func Definition(proplist def)
{
}

public func IsPlanePart() { return true; }

local Collectible = false;
local Name = "$Name$";
local Description = "$Description$";
local Rebuy = true;
local Touchable = 1; // todo: Later, this must be done with the lift tower
