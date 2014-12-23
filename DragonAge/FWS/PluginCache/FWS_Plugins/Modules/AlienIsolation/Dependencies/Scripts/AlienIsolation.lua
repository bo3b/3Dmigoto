require(GlobalDependencys:GetDependency("StandardBase"):GetPackageName())

--GAME VARS
fAdditionalFOV = 0

--ControlVars
bFixEnabled = true
bHUDFix = true

--PROCESS VARS
Process_FriendlyName = Module:GetFriendlyName()
Process_WindowName = "Alien: Isolation"
Process_ClassName = "Alien: Isolation"
Process_EXEName = "AI.exe"

--INJECTION BEHAVIOUR
InjectDelay = 5000
WriteInterval = 100
SearchInterval = 250
SuspendThread = true

--Name                         Manual/Auto/Hybrid  		Steam/Origin/Any                IncludeFile:Configure;Enable;Periodic;Disable;
SupportedVersions = { 		
{"Automatically Detect",       "Hybrid",  			"Any",	                         "Configure_SignatureScan;Enable_Inject;Periodic;Disable_Inject;"},
}

function Init_Controls()
DefaultControls.AddFixToggle("HUDFix_Enable","HUD Fix","HUDFix_Changed",25,60,180,14)
end


function Configure_SignatureScan() 

	if HackTool:GetArchitecture() == 32 then
	
		local tAddress = HackTool:AddAddress("HUDFix")
		if HackTool:SignatureScan("C7 02 ?? ?? ?? ?? 8B 54 ?? ?? C7 00 ?? ?? ?? ?? 8B 8E ?? ?? ?? ?? 89 0A 8B 86 ?? ?? ?? ??",tAddress,PAGE_EXECUTE_READ,0x0,Process_EXEName) == 0 then	
			return ErrorOccurred(string.format(SigScanError,tAddress:GetName()))
		else
			tAddress:SetAutoUnprotect(true)
			print( tAddress:GetInfo(TYPE_ADDRESS) )		
		end			
	
		local tAddress = HackTool:AddAddress("CmpAddr", 0x159EFB4)
			HackTool:GetBaseAddress()
		print( tAddress:GetInfo(TYPE_ADDRESS) )
		
		local tAddress = HackTool:AddAddress("ResAddr", 0x13547BC)
			HackTool:GetBaseAddress()
		print( tAddress:GetInfo(TYPE_ADDRESS) )
	end
	return true
end

function Enable_Inject() 


	local Variables = HackTool:AllocateMemory("Variables",0)
	Variables:PushInt("HUD_Full")
	Variables:PushInt("HUD_Width")
	Variables:PushInt("HUD_Left")	
	Variables:Allocate()	

	
	ResolutionChanged()

	local asm = [[	
	
			(codecave:jmp)HUDFix,HUDFix_cc:		
				cmp dword ptr[(address)CmpAddr], 1
				je running
				jne menus
				
				running:
				push eax
				mov eax, [(allocation)Variables->HUD_Width]
				mov [(address)ResAddr], eax
				
				mov eax, [(allocation)Variables->HUD_Left]
				mov[$$1], eax							$context=1
				jmp resolution
				
				menus:
				push eax
				mov eax, [(allocation)Variables->HUD_Full]
				mov [(address)ResAddr], eax
				mov[$$1], (int)0						$context=1
				jmp resolution
				
				resolution:
				mov eax, [(address)ResAddr]
				mov [esi + 0x000004D4], eax
				pop eax
				
				jmp %returnaddress%
				%end%				
	]]	

	
	if HackTool:CompileAssembly(asm,"HUDFix") == nil then
		return ErrorOccurred("Assembly compilation failed...")
	else
		Toggle_CodeCave("HUDFix_cc",bHUDFix)
	end	
		
end

function Periodic()
end

function Disable_Inject()
	
	CleanUp()
	
end

function ResolutionChanged() 

	local Variables = HackTool:GetAllocation("Variables")
	if Variables and Variables["HUD_Full"] and Variables["HUD_Width"] and Variables["HUD_Left"] then
		local tHUD_Full = DisplayInfo:GetVisibleWidth()
		local tHUD_Width = DisplayInfo:GetiOffsetWidth()
		local tHUD_Left = DisplayInfo:GetiOffsetX()
		Variables["HUD_Full"]:WriteInt( tHUD_Full )	
		Variables["HUD_Width"]:WriteInt( tHUD_Width )	
		Variables["HUD_Left"]:WriteInt( tHUD_Left )	
	end	

end

function HUDFix_Changed(Sender)

	bHUDFix = Toggle_CheckFix(Sender)
	Toggle_CodeCave("HUDFix_cc",bHUDFix)
	
end

function Init()	
	Init_BaseControls()
	Init_Controls()
end

function DeInit()
	DisableFix()
end