require(GlobalDependencys:GetDependency("StandardBase"):GetPackageName())

--GAME VARS
fAdditionalFOV = 0
fDefaultAspectRatio = 1.777777

--ControlVars
bFixEnabled = true
bRemoveLetterbox = true
bHUDFix = true
bMenuFOV = true
bGameFOV = true

--PROCESS VARS
Process_FriendlyName = Module:GetFriendlyName()
Process_WindowName = "*"
Process_ClassName = "*"
Process_EXEName = "ShadowOfMordor.exe"

--INJECTION BEHAVIOUR
int_ScanRetryMax = 50
InjectDelay = 5
WriteInterval = 16
SearchInterval = 25
SuspendThread = true

--Name                         Manual/Auto/Hybrid  		Steam/Origin/Any                IncludeFile:Configure;Enable;Periodic;Disable;
SupportedVersions = { 		
{"Automatically Detect",       "Hybrid",  			"Any",	                         "Configure_SignatureScan;Enable_Inject;Periodic;Disable_Inject;"},
}

function Init_Controls()
	DefaultControls.AddHeader("Header_FixesEnableDisable","Individual Fixes",245,70,210,17)
	DefaultControls.AddFixToggle("CKRemoveLetterbox_Enable","Remove Letterbox","CKRemoveLetterbox_Changed",255,101,180,14)
	DefaultControls.AddFixToggle("CKHUDFix_Enable","HUD Fix","CKHUDFix_Changed",255,120,180,14)
	DefaultControls.AddFixToggle("CKMenuFOVFix_Enable","Menu FOV Fix","CKMenuFOV_Changed",255,139,180,14)
	DefaultControls.AddFixToggle("CKGameFOVFix_Enable","Game FOV Fix","CKGameFOV_Changed",255,158,180,14)
	DefaultControls.AddHeader("Header_FOV","FOV Fine adjustment",15,70,210,17)
	DefaultControls.AddFOVSlider("FOVSlider","FOVSlider_Changed",55,100,125,35)
end


function Configure_SignatureScan() 

	local tAddress = HackTool:AddAddress("GameFOV")
	if HackTool:SignatureScan("44 0F B6 ?? ?? ?? ?? ?? 48 8B ?? ?? F3 0F 10 ?? ?? ?? F3",tAddress,PAGE_EXECUTE_READ,0xC,Process_EXEName) == 0 then	
		return ErrorOccurred(string.format(SigScanError,tAddress:GetName()))
	else
		print( tAddress:GetInfo(TYPE_ADDRESS) )		
	end		

	
	local tAddress = HackTool:AddAddress("LetterboxFix")
	if HackTool:SignatureScan("0F 84 ?? ?? ?? ?? F2 0F 10 ?? ?? ?? ?? ?? 0F 29 ?? 24 ?? ?? ?? ?? 0F 29 ?? 24",tAddress,PAGE_EXECUTE_READ,0x0,Process_EXEName) == 0 then	
		if HackTool:SignatureScan("48 89 ?? 24 ?? 44 89 ?? 24 ?? 44 89 ?? ?? 48 89 ?? ?? B9",tAddress,PAGE_EXECUTE_READ,0x17,Process_EXEName) == 0 then	
			if HackTool:SignatureScan("48 89 ?? 24 ?? 44 89 ?? 24 ?? 44 89 ?? ?? 48 89 ?? ?? 0F",tAddress,PAGE_EXECUTE_READ,0x12,Process_EXEName) == 0 then	
				return ErrorOccurred(string.format(SigScanError,tAddress:GetName()))
			else
				print( tAddress:GetInfo(TYPE_ADDRESS) )	
			end
		else
			print( tAddress:GetInfo(TYPE_ADDRESS) )		
		end	
	else
		print( tAddress:GetInfo(TYPE_ADDRESS) )				
	end

	local tAddress = HackTool:AddAddress("HUDFix")
	if HackTool:SignatureScan("0F28??F30F??87????????F30F58??????F30F11??0F28??F3",tAddress,PAGE_EXECUTE_READ,0x3,Process_EXEName) == 0 then	
		return ErrorOccurred(string.format(SigScanError,tAddress:GetName()))
	else
		print( tAddress:GetInfo(TYPE_ADDRESS) )		
	end	
	
	local tAddress = HackTool:AddAddress("MenuFOV")
	if HackTool:SignatureScan("E8????????F30F10??????????F30F59??????????E8????????488D??24??F30F59??????????0F28??E8????????4C",tAddress,PAGE_EXECUTE_READ,0xD,Process_EXEName) == 0 then	
		return ErrorOccurred(string.format(SigScanError,tAddress:GetName()))
	else
		print( tAddress:GetInfo(TYPE_ADDRESS) )		
		tAddress:AcquireAddress(0x4)		
		print( tAddress:GetInfo(TYPE_FLOAT) )		
	end		
	
	
	return true

end

function Enable_Inject() 

	local Variables = HackTool:AllocateMemory("Variables",0)
	Variables:PushFloat("HUD_SafeZoneX")
	Variables:PushFloat("HUD_SafeZoneY")	
	Variables:PushFloat("HUD_SafeZoneX2")	
	Variables:PushFloat("HUD_SafeZoneY2")
	Variables:PushFloat("AspectRatio")
	Variables:PushFloat("AdditionalFOV")
	Variables:Allocate()
	
	ResolutionChanged()
	
	local asm = [[	
		(codecave:jmp)GameFOV,GameFOV_cc:		
			%originalcode%
			addss $$1, [(allocation)Variables->AdditionalFOV]			$context=1			
			jmp %returnaddress%
			%end%		
	
		(codecave:jmp)HUDFix,HUDFix_cc:		
			movaps xmm15, [(allocation)Variables->HUD_SafeZoneX]
			movaps [$$2], xmm15				$context=1
			%originalcode%
			jmp %returnaddress%
			%end%		

		(codecave)LetterboxFix,LetterBoxFix_cc:
			jmp $$1
			nop
			%end%			
	]]	
	
	if HackTool:CompileAssembly(asm,"FOVFix") == nil then
		return ErrorOccurred("Assembly compilation failed...")
	else
		Toggle_CodeCave("LetterBoxFix_cc",bRemoveLetterbox)
		Toggle_CodeCave("HUDFix_cc",bHUDFix)
		Toggle_CodeCave("GameFOV_cc",bGameFOV)
	end	
		
end

function Periodic()

	local Variables = HackTool:GetAllocation("Variables")
	if Variables then
		if bHUDFix == true then 								
			PluginViewport:AppendStatusMessage( string.format("\r\n     (HUDScaling) SafeZoneX=%.2f, SafeZoneX2=%.2f, SafeZoneY=%.2f, SafeZoneY2=%.2f",
												Variables["HUD_SafeZoneX"]:ReadFloat(),Variables["HUD_SafeZoneX2"]:ReadFloat(),Variables["HUD_SafeZoneY"]:ReadFloat(),Variables["HUD_SafeZoneY2"]:ReadFloat()) )		
		end		
	end
	
	local MenuFOV = HackTool:GetAddress("MenuFOV")
	if bMenuFOV == true and MenuFOV then
		PluginViewport:AppendStatusMessage( string.format("\r\n     (MenuFOV) AspectRatio=%.4f",MenuFOV:ReadFloat()) )		
	end		

	if bGameFOV == true then
		if Variables["AdditionalFOV"] then
			Variables["AdditionalFOV"]:WriteFloat(fAdditionalFOV)
		end
	end
				
end

function Disable_Inject()
	
	WriteMenuFOVFix(false)
	CleanUp()
	
end


function ResolutionChanged() 

	local Variables = HackTool:GetAllocation("Variables")
	if Variables and Variables["HUD_SafeZoneX"] and Variables["HUD_SafeZoneY"] and Variables["HUD_SafeZoneX2"] and Variables["HUD_SafeZoneY2"] and Variables["AspectRatio"] then	
		local DisplayWidth = DisplayInfo:GetWidth()
		local DisplayHeight = DisplayInfo:GetHeight()
		local OffsetX = DisplayInfo:GetiOffsetX()
		local OffsetX2 = DisplayInfo:GetiOffsetX2()
		local OffsetY = DisplayInfo:GetiOffsetY()
		local OffsetY2 = DisplayInfo:GetiOffsetY2()
		local HUDWidth = DisplayInfo:GetiOffsetWidth()
		local HUDHeight = DisplayInfo:GetiOffsetHeight()
		
		local Buffer = 0.0025
		
		Variables["HUD_SafeZoneX"]:WriteFloat((OffsetX / DisplayWidth)  + Buffer)
		Variables["HUD_SafeZoneX2"]:WriteFloat((1.0 - OffsetX2 / DisplayWidth) + Buffer)
		Variables["HUD_SafeZoneY"]:WriteFloat(OffsetY / DisplayHeight)
		Variables["HUD_SafeZoneY2"]:WriteFloat((1.0 - OffsetY2 / DisplayHeight))			
		Variables["AspectRatio"]:WriteFloat(DisplayInfo:GetAspectRatio())
	end	

	WriteMenuFOVFix(bMenuFOV)
	
end

function WriteMenuFOVFix(Enabled) 

	local MenuFOV = HackTool:GetAddress("MenuFOV")
	if MenuFOV and Enabled then
		MenuFOV:WriteFloat(DisplayInfo:GetAspectRatio())		
	else
		MenuFOV:WriteFloat(fDefaultAspectRatio)
	end

end

function HK_IncreaseFOV()	
	FOVSlider:OffsetPosition(1)
end

function HK_DecreaseFOV()	
	FOVSlider:OffsetPosition(-1)
end

function FOVSlider_Changed(Sender)
	fAdditionalFOV = Sender:GetScaledFloat(4)
	lblFOVSlider.Caption:SetCaption( string.format("Value: %.2f",fAdditionalFOV) )	
	ForceUpdate()			
end

function CKMenuFOV_Changed(Sender)
	bMenuFOV = Toggle_CheckFix(Sender)
	WriteMenuFOVFix(bMenuFOV)
	ForceUpdate()	
end

function CKHUDFix_Changed(Sender)
	bHUDFix = Toggle_CheckFix(Sender)
	Toggle_CodeCave("HUDFix_cc",bHUDFix)	
	ForceUpdate()	
end

function CKGameFOV_Changed(Sender)
	bGameFOV = Toggle_CheckFix(Sender)
	Toggle_CodeCave("GameFOV_cc",bGameFOV)	
	ForceUpdate()	
end

function CKRemoveLetterbox_Changed(Sender)
	bRemoveLetterbox = Toggle_CheckFix(Sender)
	Toggle_CodeCave("LetterBoxFix_cc",bRemoveLetterbox)	
	ForceUpdate()	
end

function Init()	
	Init_BaseControls()
	Init_Controls()
end

function DeInit()
	DisableFix()
end