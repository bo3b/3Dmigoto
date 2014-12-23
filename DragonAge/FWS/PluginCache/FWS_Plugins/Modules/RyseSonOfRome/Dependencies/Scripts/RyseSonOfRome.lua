require(GlobalDependencys:GetDependency("StandardBase"):GetPackageName())

--GAME VARS
fAdditionalFOV = 0

--ControlVars
bFixEnabled = true
bHUDFix = true

--PROCESS VARS
Process_FriendlyName = Module:GetFriendlyName()
Process_WindowName = "Ryse (TM)"
Process_ClassName = "CryENGINE"
Process_EXEName = "Ryse.exe"

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

	if HackTool:GetArchitecture() == 64 then
	
		local tAddress = HackTool:AddAddress("HUDFix")
		if HackTool:SignatureScan("66 45 0F 6E 44 24 ?? F3 0F 10 2D ?? ?? ?? ?? F3 44 ?? ?? 1D ?? ?? ?? ?? 0F 5B C0 45 0F 5B C0",tAddress,PAGE_EXECUTE_READ,0x0,Process_EXEName) == 0 then	
			return ErrorOccurred(string.format(SigScanError,tAddress:GetName()))
		else
			tAddress:SetAutoUnprotect(true)
			print( tAddress:GetInfo(TYPE_ADDRESS) )		
		end			
	end
	return true
end

function Enable_Inject() 

	local Variables = HackTool:AllocateMemory("Variables",0)
	Variables:PushInt("HUD_Width")
	Variables:PushInt("HUD_Left")	
	Variables:Allocate()	
	
	ResolutionChanged()

	local asm = [[	
	
			(codecave:jmp)HUDFix,HUDFix_cc:		
				push rcx
				mov ecx, [(allocation)Variables->HUD_Width]
				mov [$$2], ecx										$context=1
				mov ecx, [(allocation)Variables->HUD_Left]
				mov [$$2-0x8], ecx									$context=1
				pop rcx
				%originalcode%
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
	if Variables and Variables["HUD_Width"] and Variables["HUD_Left"] then
		local tHUD_Width = DisplayInfo:GetiOffsetWidth()
		local tHUD_Left = DisplayInfo:GetiOffsetX()
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