require(GlobalDependencys:GetDependency("StandardBase"):GetPackageName())

--GAME VARS
fAdditionalFOV = 0

--ControlVars
bFixEnabled = true
bArFix = true

--PROCESS VARS
Process_FriendlyName = Module:GetFriendlyName()
Process_WindowName = "Dragon Age: Inquisition"
Process_ClassName = "Dragon Age: Inquisition"
Process_EXEName = "DragonAgeInquisition.exe"

--INJECTION BEHAVIOUR
InjectDelay = 0
WriteInterval = 100
SearchInterval = 250
SuspendThread = true

--Name                         Manual/Auto/Hybrid  		Steam/Origin/Any                IncludeFile:Configure;Enable;Periodic;Disable;
SupportedVersions = { 		
{"Automatically Detect",       "Hybrid",  			"Any",	                         "Configure_SignatureScan;Enable_Inject;Periodic;Disable_Inject;"},
}

function Init_Controls()
DefaultControls.AddFixToggle("AspectRatioFix_Enable","AR Fix","AspectRatio_Changed",25,60,180,14)
end

function Configure_SignatureScan() 

	if HackTool:GetArchitecture() == 64 then
		local tAddress = HackTool:AddAddress("ArFix", HackTool:GetBaseAddress(), 0x3B9CC93)
	end
	return true
end

function Enable_Inject() 

	local Variables = HackTool:AllocateMemory("Variables",0)
	Variables:PushFloat("AspectRatio")
	Variables:Allocate()	
	ResolutionChanged()

	local asm = [[	
	
			(codecave:jmp)ArFix,ArFix_cc:
				push rax
				mov rax, [(allocation)Variables->AspectRatio]
				mov [rcx+0x134], rax
				pop rax
				jmp %returnaddress%
				%end%				
	]]	

	
	if HackTool:CompileAssembly(asm,"ArFix") == nil then
		return ErrorOccurred("Assembly compilation failed...")
	else
		Toggle_CodeCave("ArFix_cc",bArFix)
	end	
		
end

function Periodic()
end

function Disable_Inject()
	
	CleanUp()
	
end

function ResolutionChanged() 

	local Variables = HackTool:GetAllocation("Variables")
	if Variables and Variables["AspectRatio"] then
		local tAspectRatio = DisplayInfo:GetAspectRatio()
		Variables["AspectRatio"]:WriteFloat( tAspectRatio )	
	end	

end

function AspectRatio_Changed(Sender)

	bArFix = Toggle_CheckFix(Sender)
	Toggle_CodeCave("ArFix_cc",bArFix)
	
end

function Init()	
	Init_BaseControls()
	Init_Controls()
end

function DeInit()
	DisableFix()
end