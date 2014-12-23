if ModuleDependencys:GetDependency("Theme") then
	require(ModuleDependencys:GetDependency("Theme"):GetPackageName())
else
	require(GlobalDependencys:GetDependency("StandardTheme"):GetPackageName())
end

int_tOriginalEXEName = nil
int_tClassNames = nil
int_tEXENames = nil
int_FailureRetryInterval = 60
int_CurrentRetryInterval = 0
int_ViewportStarted = false


function Init_BaseControls()

	--Module Vars
	SyncDisplayDetection()
	HackTool = FWSBinds.c_HackTool()
	HackTool:SetProcessFriendlyName(Process_FriendlyName)
	WindowTool = FWSBinds.c_WindowTools()			
	
	if Process_EXEName == nil then Process_EXEName = "*" end
	if Process_ClassName == nil then Process_ClassName = "*" end
	if Process_WindowName == nil then Process_WindowName = "*" end
	
	int_WindowNames = split(Process_WindowName,";")
	int_ClassNames = split(Process_ClassName,";")
	int_EXENames = split(Process_EXEName,";")
	
	--UI VARS
	VersionString = "Unknown"
	bAutoDetectVersion = true
	bVersionSupported = false
	bIsSteam = false
	bIsOrigin = false

	--INJECTION VARS
	RequreTarget = nil
	ConfigureFunction = nil
	EnableFunction = nil
	DisableFunction = nil
	PeriodicFunction = nil
	PreviouslyEnabled = false
	WaitingForProcessStart = false
	LastKnownPID = 0
	
	if SuspendThread == nil then SuspendThread = true end
	if InjectDelay == nil then InjectDelay = 1 end
	if SearchInterval == nil then SearchInterval = 500 end
	if ConfigureDelay == nil then ConfigureDelay = 0 end
	if int_ScanRetryMax == nil then int_ScanRetryMax = 10 end

	SigScanError = string.format("Could not find %%s injection point, %s may have updated to a version that is no longer supported.\r\n\r\nTry restarting %s or selecting a different version and re-enable the fix.",Process_FriendlyName,Process_FriendlyName)
	
	DefaultControls.AddHeader("Header_EnableDisable","Enable / Disable",15,10,210,17)	
	DefaultControls.AddFixToggle("CheckBox_Enable","Fix Enabled","CKEnable_Changed",25,41,100,14,true)	
	DefaultControls.AddHeader("Header_Version","Game Version",245,10,210,17)
	
	VersionOptions = {}
	for index,value in ipairs(SupportedVersions) do
		if value[2] ~= "Auto" then
			VersionOptions[index] = string.format("%s (%s)",value[1],value[2])
		else
			VersionOptions[index] = value[1]
		end
	end	
	
	DefaultControls.AddComboBox("VersionCombo","VersionCombo_Changed",VersionOptions,255,37,190,300)
	DefaultControls.AddTimer("Fix_Timer",60000,"Fix_Timer_Elapsed",SearchInterval,false)
	DefaultControls.AddTimer("Failed_Fix_Timer",60001,"Failed_Fix_Timer_Elapsed",1000,false)	

	Fix_Timer:SetEnabled(true)	
	
end

function HK_EnableToggle()
	
	if bFixEnabled == true then 	
		CheckBox_Enable:SetCheckState(0)
		CKEnable_Changed(CheckBox_Enable)
	else
	
		CheckBox_Enable:SetCheckState(1)	
		CKEnable_Changed(CheckBox_Enable)
	end
	
end

function CKEnable_Changed(Sender)

	Failed_Fix_Timer:SetEnabled(false)
	
	if Toggle_CheckFix(Sender) == true then
		bFixEnabled = true	
		PluginViewport:SetStatusMessage( "Searching for " .. Process_FriendlyName .. " process, please configure desired settings and launch the game." ,true )
	else
		bFixEnabled = false
	end

	Fix_Timer:SetEnabled(bFixEnabled)
	Fix_Timer:SetInterval(SearchInterval)
	ForceUpdate()

end

function VersionCombo_Changed(Sender)

	if HackTool:GetIsValid() == false then return false end
	
	if LastKnownPID == HackTool:GetAttachedProcessID() then 
		DisableFix()	
	end	
	
	CleanUp()
	
	if int_tOriginalEXEName ~= nil then
		Process_EXEName = int_tOriginalEXEName
		int_tOriginalEXEName = nil
	end

	local tProcessName = HackTool:GetProcessEXEName()
	if Process_EXEName ~= tProcessName then		
	    int_tOriginalEXEName = Process_EXEName
		Process_EXEName = tProcessName
	else 
		int_tOriginalEXEName = nil
	end	
	
	int_ScanRetryCount = 1
	PreviouslyEnabled = false
	
	vString = Sender:GetSelectedString()
	
	local DetectedVersion = HackTool:DetectProcessVersion()	
	bIsSteam = HackTool:GetIsSteamApp()
	if bIsSteam == false then
		bIsOrigin = HackTool:GetIsOriginApp()		
	end
	
	local RefString = nil	
	
	for index,value in ipairs(SupportedVersions) do	
	
		if value[2] ~= "Auto" then
			RefString = string.format("%s (%s)",value[1],value[2])
		else
			RefString = value[1]
		end	
		
		if RefString == vString then
		
			local VersionFound = false						
			
			local DetectType = 0			
			local Functions = 0
			
			if string.find(value[4],":") ~= nil then			
				RequreTarget = string.sub(value[4],1,string.find(value[4],":")-1)
				Functions = string.sub(value[4],string.find(value[4],":")+1)
			else
				Functions = value[4]
			end
			
			local LastPos = 0
			local ArgCount = 0

			while LastPos ~= nil do				
				ArgCount = ArgCount + 1				
				if ArgCount == 1 then
					strConfigureFunction = string.sub(Functions,LastPos,string.find(Functions,";",LastPos+1)-1)
				elseif ArgCount == 2 then
					strEnableFunction = string.sub(Functions,LastPos+1,string.find(Functions,";",LastPos+1)-1)
				elseif ArgCount == 3 then
					strPeriodicFunction = string.sub(Functions,LastPos+1,string.find(Functions,";",LastPos+1)-1)
				elseif ArgCount == 4 then
					strDisableFunction = string.sub(Functions,LastPos+1,string.find(Functions,";",LastPos+1))
					if string.find(strDisableFunction,";") then strDisableFunction = string.sub(strDisableFunction,1,string.len(strDisableFunction)-1) end
				end				
				LastPos = string.find(Functions,";",LastPos+1)
			end	
::configureretry::
			local ConfigureSuccess = false
			local EnableSuccess = false
			
			if strConfigureFunction ~= nil then		
				if _G[strConfigureFunction] then 
					ConfigureFunction = _G[strConfigureFunction]
					 PluginViewport:SetStatusMessage( "Configuring..." ,true )	
					 if ConfigureDelay ~= nil and ConfigureDelay > 0 then
						print("Delaying signature scan...")
						HiResSleep(ConfigureDelay)
					 end
					 
					if SuspendThread == true then SuspendedThreadID = HackTool:SuspendAttachedThread() end						
					ConfigureSuccess = ConfigureFunction()	
					if SuspendThread == true then HackTool:ResumeThread(SuspendedThreadID) end	
					
				end
			end
			
			if ConfigureSuccess == true then

				if strEnableFunction ~= nil then
					if _G[strEnableFunction] then EnableFunction = _G[strEnableFunction] end
				end	
					
				if strDisableFunction ~= nil then
					if _G[strDisableFunction] then	DisableFunction = _G[strDisableFunction] end
				end		
					
				if strPeriodicFunction ~= nil then
					if _G[strPeriodicFunction] then PeriodicFunction = _G[strPeriodicFunction] end
				end
			else
			
				if SuspendThread == true then HackTool:ResumeThread(SuspendedThreadID) end
			
				if int_ScanRetryCount < int_ScanRetryMax+1 then

					if HackTool:GetIsValid() == false then
						return false
					end
					
					Sleep(50)
					print(string.format("Retrying, %d of %d",int_ScanRetryCount,int_ScanRetryMax))
					CleanUp()
					int_ScanRetryCount = int_ScanRetryCount + 1
					goto configureretry
				else 
					return false
				end
				
			end
			
			if value[2] == "Auto" then DetectType = 1 elseif value[2] == "Hybrid" then DetectType = 2 else DetectType = 3 end
			
			if DetectedVersion ~= "Unknown" then
				VersionFound = true			
			else
				VersionFound = false
			end			
			
			local VersionString = string.format("%s (%s) -> Detected: %s",value[1],value[2],DetectedVersion)
			if bIsSteam == true then VersionString = VersionString .. " (Steam)" 
			elseif bIsOrigin == true then VersionString = VersionString .. " (Origin)" end			

			HackTool:SetProcessVersion(VersionString)
			
		end
	end		

	
 
	if WaitingForProcessStart == true or LastKnownPID ~= HackTool:GetAttachedProcessID() then
		if InjectDelay > 0 then
			PluginViewport:SetStatusMessage( "Injection delay... waiting for process to stabilise, Please wait...\r\n\r\nPlease note, terminating Flawless Widescreen or the LUA Virtual Machine may result in undefined behaviour." , true )	
			local ConfigureEnd = os.clock()
			print(string.format("Inject delay: %.0fms, Configure() took: %.0fms, Adjusted Inject delay: %.0fms", InjectDelay, (ConfigureEnd - ConfigureStart)*1000,InjectDelay - (ConfigureEnd - ConfigureStart)*1000))		
			ConfigureEnd = os.clock()
			HiResSleep(InjectDelay - (ConfigureEnd - ConfigureStart)*1000)
			local Difference = os.clock()
			print(string.format("Actual delay: %.0fms", (Difference - ConfigureEnd)*1000))	
		end
	end

	PluginViewport:SetStatusMessage( "Injecting..." ,true )	
	
	if SuspendThread == true then SuspendedThreadID = HackTool:SuspendAttachedThread() end
	EnableFunction()	
	if SuspendThread == true then HackTool:ResumeThread(SuspendedThreadID) end	
	
	Fix_Timer:SetInterval(WriteInterval)
	Fix_Timer:SetEnabled(true)	
	ForceUpdate()
	
	return true
	
end

function DisableFix()	

	if DisableFunction ~= nil then
		if WaitingForProcessStart == false then
			DisableFunction() 
		end
	 end
 
 	DisableFunction = nil
	PeriodicFunction = nil
	EnableFunction = nil
	ConfigureFunction = nil	

	CleanUp()
	
end

function CleanUp()

	HackTool:DeleteCodeCaves()
	HackTool:DeleteAllocations()
	HackTool:DeleteAddresses()	
	HackTool:DeletePointerChains()

end

function DisplayDetection_ResolutionChanged() 
	
	SyncDisplayDetection()

	if ResolutionChanged ~= nil then
		ResolutionChanged()
	end
	
end

function Failed_Fix_Timer_Elapsed(Sender,Argument)	
	
	int_CurrentRetryInterval = int_CurrentRetryInterval + 1
	if int_CurrentRetryInterval > int_FailureRetryInterval then 
		Sender:SetEnabled(false)
		int_CurrentRetryInterval = 0
		CheckBox_Enable:SetCheckState(1)
		CKEnable_Changed(CheckBox_Enable)
	else
		PluginViewport:SetStatusMessage( string.format("Configure function call for the attached process failed, retrying in %d seconds...\r\n\r\nLast Error:\r\n%s",int_FailureRetryInterval - int_CurrentRetryInterval,int_LastError),true )
	end
	
	
end

function Fix_Timer_Elapsed(Sender,Argument)

	if bFixEnabled == true then

		if HackTool:GetIsValid() == true then		
			PluginViewport:SetStatusMessage( HackTool:GetProcessSummary() )

			if DisplayInfo and DisplayGridDetails then				
				PluginViewport:AppendStatusMessage( DisplayInfo:GetDisplaySummary() )
				if PeriodicFunction ~= nil then PeriodicFunction() end
			end			

			PluginViewport:RenderStatusMessage()
		
		else
			
			if FWS_LoadingState == true then
				PluginViewport:SetStatusMessage( "Loading state information..." ,true )		
				return				
			else 
			
				local ProcessFound = false
				
				if #int_ClassNames == 1 and #int_WindowNames == 1 and int_ClassNames[1] == "*" and int_WindowNames[1] == "*" then
					for key,exename in pairs(int_EXENames) do
						if HackTool:FindProcess("*",exename,"*") == true then	
							if HackTool:OpenProcess(PROCESS_ALL_ACCESS) == true then
								Fix_Timer:SetEnabled(false)
								ProcessFound = true
								break							
							end
						end						
					end
				else

					for key,classname in pairs(int_ClassNames) do
						for key,windowname in pairs(int_WindowNames) do	
							if HackTool:FindProcess(windowname,Process_EXEName,classname) == true then									
								if HackTool:OpenProcess(PROCESS_ALL_ACCESS) == true then
									Fix_Timer:SetEnabled(false)
									ProcessFound = true
									break							
								end
							end
						end
						if ProcessFound == true then break end
					end								
				end
				
				if ProcessFound == false then
					if WaitingForProcessStart == false then
						PluginViewport:SetStatusMessage( "Searching for " .. Process_FriendlyName .. " process, please configure desired settings and launch the game." ,true )					
						WaitingForProcessStart = true
						Fix_Timer:SetInterval(SearchInterval)
					end
				else
					WindowTool:AttachToWindow(HackTool:GetAttachedHWND())
				
					ConfigureStart = os.clock()
					if VersionCombo_Changed(VersionCombo) == false then
						StartFailureCountdown() 
					else
						WaitingForProcessStart = false
						LastKnownPID = HackTool:GetAttachedProcessID()
					end	
				end
				
			end		

		end
		
	else
		DisableFix()
		Sender:SetEnabled(false)
		Fix_Timer:SetInterval(SearchInterval)
		HackTool:CloseProcess()
		PluginViewport:SetStatusMessage( Process_FriendlyName .. " Fix disabled - Click \"Fix Enabled\" to enable." , true)
	end
	
end

function StartFailureCountdown() 
	Fix_Timer:SetEnabled(false)
	int_CurrentRetryInterval = 0
	Failed_Fix_Timer:SetEnabled(true)
	HackTool:CloseProcess()
end

function ErrorOccurred(error_text,fatel)
	
	if int_ScanRetryCount > int_ScanRetryMax or fatel ~= nil and fatel == true then
		print("Fatel error.")
		CheckBox_Enable:SetCheckState(0)
		CKEnable_Changed(CheckBox_Enable)
		PluginViewport:SetStatusMessage( error_text , true)
		int_LastError = error_text
	else 
		print("Non-fatel error, retrying.")
	end
	
	
	return false
end

function ForceUpdate()
	Fix_Timer_Elapsed(Fix_Timer,nil)
end

function ForcePeriodic()
	if PeriodicFunction ~= nil then PeriodicFunction() end
end

function Write_CodeCave(CodeCaveName)
	local CodeCave = HackTool:GetCodeCave(CodeCaveName)
	if CodeCave ~= nil then CodeCave:WriteData() end
end

function Restore_CodeCave(CodeCaveName)
	local CodeCave = HackTool:GetCodeCave(CodeCaveName)
	if CodeCave ~= nil then	CodeCave:Restore() end
end

function Toggle_CodeCave(CodeCaveName, ToggleBool)
	local CodeCave = HackTool:GetCodeCave(CodeCaveName)
	if CodeCave ~= nil then
		if ToggleBool == true then
			CodeCave:WriteData()
		else
			CodeCave:Restore()
		end
		
	end
end

function SyncDisplayDetection()
	DisplayInfo = DisplayDetection:GetPreferredDisplay()
	DisplayGridDetails = DisplayInfo:GetGridSize()
	
	--block for a while incase the object is invalid
	while DisplayInfo == nil or DisplayGridDetails == nil do
		DisplayInfo = DisplayDetection:GetPreferredDisplay()
		DisplayGridDetails = DisplayInfo:GetGridSize()
	end
end

function Round(what, precision)
   return FWSBinds.c_Tools_Round(what,precision)
end

function GetContextAspectDevisional()

	if fDefaultAspectRatio ~= nil then
		return DisplayInfo:GetAspectRatio() / fDefaultAspectRatio
	end
		
	return DisplayInfo:GetAspectRatio() / 1.777777791
end

function GetContextFOVMax()

	if fFOVMax ~= nil and fDefaultAspectRatio ~= nil then
		local FOVMax = DisplayInfo:GetAdjustedFOV(fFOVMax,fDefaultAspectRatio,0,179)
		return FOVMax
	end
	
	return DisplayInfo:GetAdjustedFOV(110,1.777777791,0,179)
	
end

function UpdateFOVCalculator_AdditionalFOV(AllocationName, AdditionalFOV)

	local FOVCalc = HackTool:GetAllocation(AllocationName)
	if FOVCalc then
		local OffsetAdditionalFOV = FOVCalc["AdditionalFOV"]
		if OffsetAdditionalFOV then OffsetAdditionalFOV:WriteFloat(AdditionalFOV) end
	end

end

function UpdateFOVCalculator_AspectDevisional(AllocationName, AspectDevisional)

	local FOVCalc = HackTool:GetAllocation(AllocationName)
	if FOVCalc then 
		local OffsetAspectDevisional = FOVCalc["AspectDevisional"]
		if OffsetAspectDevisional then OffsetAspectDevisional:WriteFloat(AspectDevisional) end
	end	
	
end

function UpdateFOVCalculator_MaxFOV(AllocationName, MaxFOV)

	local FOVCalc = HackTool:GetAllocation(AllocationName)
	if FOVCalc then 
		local OffsetFOVMaximum = FOVCalc["FOVMaximum"]
		if MaxFOV == nil then
			if OffsetFOVMaximum then OffsetFOVMaximum:WriteFloat(GetContextFOVMax()) end
		else
			if OffsetFOVMaximum then OffsetFOVMaximum:WriteFloat(MaxFOV) end
		end		
	end	
	
end

function UpdateFOVCalculator(AllocationName, AspectDevisional, AdditionalFOV, MaxFOV) 
	local FOVCalc = HackTool:GetAllocation(AllocationName)
	if FOVCalc then 
		local OffsetAdditionalFOV = FOVCalc["AdditionalFOV"]
		if OffsetAdditionalFOV then OffsetAdditionalFOV:WriteFloat(AdditionalFOV) end
		local OffsetAspectDevisional = FOVCalc["AspectDevisional"]
		if OffsetAspectDevisional then OffsetAspectDevisional:WriteFloat(AspectDevisional) end
		local OffsetFOVMaximum = FOVCalc["FOVMaximum"]
		
		if MaxFOV == nil then
			if OffsetFOVMaximum then OffsetFOVMaximum:WriteFloat(GetContextFOVMax()) end
		else
			if OffsetFOVMaximum then OffsetFOVMaximum:WriteFloat(MaxFOV) end
		end				
		
				
	end	
end

function FOVCalculatorSummary(AllocationName, Title, OptionalBool, ShowAdditional)
	if OptionalBool ~= nil and OptionalBool == false then return string.format("     (%s FOV) - Disabled",Title) end
	
	local FOVCalc = HackTool:GetAllocation(AllocationName)
	if FOVCalc then 	
		local OffsetOriginalFOV = FOVCalc["OriginalFOV"]
		local OffsetFOVResult = FOVCalc["FOVResult"]
		local OffsetAdditionalFOV = FOVCalc["AdditionalFOV"]
		local OffsetMaxFOV = FOVCalc["FOVMaximum"]
		
		if OffsetOriginalFOV and OffsetFOVResult and OffsetAdditionalFOV and OffsetMaxFOV then
			local Additional = OffsetAdditionalFOV:ReadFloat()
			local MaxFOV = OffsetMaxFOV:ReadFloat()
			local OutputFOV = OffsetFOVResult:ReadFloat()
			
			local LimitingIndicator = ""			
			if (OutputFOV+0.5) > MaxFOV then
				LimitingIndicator = "[!!]"
			elseif (OutputFOV+2.0) > MaxFOV then
				LimitingIndicator = "[!]"
			end
			
			if Additional > 0 or Additional == 0 then
				return string.format("     (%s FOV) - In: %.2f, Out: %.2f (+%.2f) %s",Title,OffsetOriginalFOV:ReadFloat(),OutputFOV, Additional, LimitingIndicator)
			else 
				return string.format("     (%s FOV) - In: %.2f, Out: %.2f (%.2f) %s",Title,OffsetOriginalFOV:ReadFloat(),OutputFOV, Additional, LimitingIndicator)
			end
			
		end					

	end	
	
	return string.format("     (%s FOV) - Error",Title)
end

function Toggle_CheckFix(Control)
	
	local Result = false

	if Control:GetCheckState() > 0 then			
		Result = true
	else
		Result = false
	end
	
	ApplyThemeToControl(Control,Theme.FixToggle)

	return Result
	
end

function split (s, delim)

	if delim ~= nil and string.find(s,delim) ~= nil then
	  local start = 1
	  local t = {}

	  while true do
		local pos = string.find (s, delim, start, true)
		if not pos then break end
		table.insert (t, string.sub (s, start, pos - 1))
		start = pos + string.len (delim)
	  end 

	  table.insert (t, string.sub (s, start))
	  return t		
	else
		return {s}
	end

end 

DefaultControls = {}

DefaultControls.AddHeader = function(ControlName, Label, Pos_X, Pos_Y, Width, Height)
	_G[ControlName] = FWSBinds.c_Label()
	local tLabel = _G[ControlName]
	tLabel:SetRect(Pos_X,Pos_Y,Width,Height)
	tLabel:SetBorder(true)
	tLabel.Caption:SetCaption( "   +  " .. Label )
	ApplyThemeToControl(tLabel,Theme.Header)
	ContainerChild:RegisterControl(tLabel,ControlName)	
	return tLabel
end

DefaultControls.AddWarning = function(ControlName, Label, Pos_X, Pos_Y)
	_G[ControlName] = FWSBinds.c_Label()
	local tLabel = _G[ControlName]
	tLabel:SetRect(Pos_X,Pos_Y,440,70)
	tLabel:SetAlignCenter()
	tLabel:SetBorder(true)
	tLabel.Caption:SetCaption(Label)
	ApplyThemeToControl(tLabel,Theme.Warning)
	ContainerChild:RegisterControl(tLabel,ControlName)	
	
	DefaultControls.AddFixToggle("CK" .. ControlName,"Acknowledge and Ignore","CK" .. ControlName .. "_Changed",Pos_X+135,Pos_Y+75,180,14,false)
	return tLabel
end

DefaultControls.AddFixedFOVSlider = function(ControlName, ChangeEvent, Pos_X, Pos_Y, Width, Height, LowValue, HighValue, DefaultValue, Scale)
	_G[ControlName] = FWSBinds.c_TrackBar()
	local FOVSlider = _G[ControlName]
	FOVSlider:SetRect(Pos_X,Pos_Y,Width,Height)
	FOVSlider:SetLabel1Text(LowValue)
	FOVSlider:SetLabel2Text(HighValue)
	FOVSlider:SetLabel1Enabled(true)
	FOVSlider:SetLabel2Enabled(true)
	
	if Scale ~= nil then
		FOVSlider:SetTickFrequency(Scale)
		FOVSlider:SetRange(LowValue*Scale,HighValue*Scale)	
		
		if DefaultValue ~= nil then
			FOVSlider:SetPosition(DefaultValue*Scale)		
		else
			FOVSlider:SetPosition((LowValue + ((HighValue - LowValue) / 2)) * Scale)
		end			
	else
		FOVSlider:SetTickFrequency(1)
		FOVSlider:SetRange(LowValue,HighValue)	
		
		if DefaultValue ~= nil then
			FOVSlider:SetPosition(DefaultValue)		
		else
			FOVSlider:SetPosition(LowValue + ((HighValue - LowValue) / 2))
		end				
	end

	local tLabel = FOVSlider:GetLabel1()
	ApplyThemeToControl(tLabel,Theme.FOVSlider.Labels)
	tLabel:SetRect(0,0,50,17)

	local tLabel = FOVSlider:GetLabel2()
	ApplyThemeToControl(tLabel,Theme.FOVSlider.Labels)
	tLabel:SetRect(0,0,50,17)

	
	
	FOVSlider.Events:SetLua_ValueChanged(ChangeEvent)	
	FOVSlider:SetSaveState(true)	
	ContainerChild:RegisterControl(FOVSlider,ControlName)	
	
	local LabelName = "lbl" .. ControlName
	_G[LabelName] = FWSBinds.c_Label()
	local lblFOVSlider = _G[LabelName]
	lblFOVSlider:SetRect(Pos_X+(Width/2)-(75/2),Pos_Y+35,75,17)
	ApplyThemeToControl(lblFOVSlider,Theme.FOVSlider.ValueLabel)
	lblFOVSlider.Caption:SetCaption( "Value: 0.00" )
	lblFOVSlider:SetAlignCenter()
	ContainerChild:RegisterControl(lblFOVSlider,LabelName)	
	
	if DefaultValue ~= nil then
		_G[ChangeEvent](FOVSlider)
	end	

	return FOVSlider
end

DefaultControls.AddFOVSlider = function(ControlName, ChangeEvent, Pos_X, Pos_Y, Width, Height, DefaultValue)
	_G[ControlName] = FWSBinds.c_TrackBar()
	local FOVSlider = _G[ControlName]
	FOVSlider:SetRect(Pos_X,Pos_Y,Width,Height)
	FOVSlider:SetLabel1Text("Lower")
	FOVSlider:SetLabel2Text("Higher")
	FOVSlider:SetLabel1Enabled(true)
	FOVSlider:SetLabel2Enabled(true)
	FOVSlider:SetTickFrequency(15)
	FOVSlider:SetRange(0,200)

	local tLabel = FOVSlider:GetLabel1()
	ApplyThemeToControl(tLabel,Theme.FOVSlider.Labels)
	tLabel:SetRect(0,0,50,17)

	local tLabel = FOVSlider:GetLabel2()
	ApplyThemeToControl(tLabel,Theme.FOVSlider.Labels)
	tLabel:SetRect(0,0,50,17)

	if DefaultValue ~= nil then
		FOVSlider:SetPosition(DefaultValue)		
	else
		FOVSlider:SetPosition(100)
	end		
	
	FOVSlider.Events:SetLua_ValueChanged(ChangeEvent)	
	FOVSlider:SetSaveState(true)	
	ContainerChild:RegisterControl(FOVSlider,ControlName)	
	
	local LabelName = "lbl" .. ControlName
	_G[LabelName] = FWSBinds.c_Label()
	local lblFOVSlider = _G[LabelName]
	lblFOVSlider:SetRect(Pos_X+(Width/2)-(75/2),Pos_Y+35,75,17)
	ApplyThemeToControl(lblFOVSlider,Theme.FOVSlider.ValueLabel)
	lblFOVSlider.Caption:SetCaption( "Value: 0.00" )
	lblFOVSlider:SetAlignCenter()
	ContainerChild:RegisterControl(lblFOVSlider,LabelName)	
	
	if DefaultValue ~= nil then
		_G[ChangeEvent](FOVSlider)
	end	

	return FOVSlider
end

DefaultControls.AddFixToggle = function(ControlName, Caption, ChangeEvent, Pos_X, Pos_Y, Width, Height, DefaultValue) 

	_G[ControlName] = FWSBinds.c_CheckBox()
	local CKToggle = _G[ControlName]
	CKToggle:SetRect(Pos_X,Pos_Y,Width,Height)
	CKToggle.Caption:SetCaption(Caption)
	
	if DefaultValue ~= nil and DefaultValue == false then
		CKToggle:SetCheckState(0)
	else
		CKToggle:SetCheckState(1)
	end	

	ApplyThemeToControl(CKToggle,Theme.FixToggle)
	
	CKToggle.Events:SetLua_CheckStateChanged(ChangeEvent)	
	CKToggle:SetSaveState(true)
	ContainerChild:RegisterControl(CKToggle,ControlName)	
	return CKToggle
end

DefaultControls.AddParameterBox = function(ControlName, Caption, ChangeEvent, Pos_X, Pos_Y, Width, Height) 

	_G[ControlName] = FWSBinds.c_InputBox()
	local EBControl = _G[ControlName]
	EBControl:SetRect(Pos_X,Pos_Y,Width,Height)	
	EBControl:SetBorder(true)
	EBControl:SetAlignCenter()
	ApplyThemeToControl(EBControl,Theme.ParameterBox)
	EBControl.Events:SetLua_TextChanged(ChangeEvent)	
	EBControl:SetSaveState(true)
	EBControl:SetText(Caption)
	ContainerChild:RegisterControl(EBControl,ControlName)	
	return EBControl
	
end

DefaultControls.AddTimer = function (ControlName, TimerID, ElapsedFunction, Interval, Enabled)
	
	_G[ControlName] = FWSBinds.c_Timer()
	local tTimer = _G[ControlName]
	tTimer:SetTimerID(TimerID)
	tTimer.Events:SetLua_TimerElapsed(ElapsedFunction)
	tTimer:SetInterval(Interval)
	tTimer:SetEnabled(Enabled)
	ContainerChild:RegisterControl(tTimer,ControlName)
	return tTimer
	
end

DefaultControls.AddComboBox = function (ControlName,SelectionChangedEvent,Options,Pos_X, Pos_Y, Width, Height)

	_G[ControlName] = FWSBinds.c_ComboBox()
	local tComboBox = _G[ControlName]
	tComboBox:SetRect(Pos_X,Pos_Y,Width,Height)
	
	for index,value in ipairs(Options) do
		tComboBox:InsertItem(Options[index])
	end
	
	ApplyThemeToControl(tComboBox,Theme.ComboBox)

	tComboBox:SetSelectedIndex(0)
	tComboBox:SetDropDownReadOnly()
	tComboBox.Events:SetLua_IndexChanged(SelectionChangedEvent)
	tComboBox:SetSaveState(true)		
	ContainerChild:RegisterControl(tComboBox,ControlName)	

	return tComboBox
end

DefaultControls.AddButton = function(ControlName,Caption,ButtonClickEvent,Pos_X,Pos_Y,Width,Height,StepType)

	_G[ControlName] = FWSBinds.c_Button()
	local tButton = _G[ControlName]
	
	tButton:SetRect(Pos_X,Pos_Y,Width,Height)
	
	tButton.Caption:SetCaption(Caption)
	tButton.Events:SetLua_ButtonClick(ButtonClickEvent)
	
	ApplyThemeToControl(tButton,Theme.Button)

	if StepType ~= nil then
		local SetRect = false
		
		if StepType == "Next" then 
			tButton:SetType_NextStep() 
			SetRect = true
		elseif StepType == "Prev" then 		
			tButton:SetType_PrevStep() 
			SetRect = true
		elseif StepType == "Exec" then 
			tButton:SetType_ExecStep() 
			SetRect = true			
		end
		
		if SetRect == true then
			tButton:SetRect(Pos_X,Pos_Y,125,44)
		end
	end
	
	ContainerChild:RegisterControl(tButton,ControlName)
	return tButton
	
end

