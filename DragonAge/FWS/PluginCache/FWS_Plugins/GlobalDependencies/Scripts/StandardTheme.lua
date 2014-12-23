RGBToHex = FWSBinds.c_Tools_RGBtoHex

function ApplyThemeToControl(Control,Theme) 

	if Theme == nil or Control == nil then return end

	local ControlObject = Control.Color
	if ControlObject then
		local tTheme = Theme.Color		
		if tTheme then			
			if tTheme.Backcolor then ControlObject:SetBackColor(tTheme.Backcolor) end
			if tTheme.Forecolor then ControlObject:SetForeColor(tTheme.Forecolor) end	
			if tTheme.Alpha then ControlObject:SetBackColorAlpha(tTheme.Alpha) end	
			if Control.GetCheckState then
				if tTheme.Checked_Forecolor then 
					if Control:GetCheckState() == 0 then ControlObject:SetForeColor(tTheme.Checked_Forecolor) end				 
				end
				if tTheme.Unchecked_Forecolor then 
					if Control:GetCheckState() == 1 then ControlObject:SetForeColor(tTheme.Unchecked_Forecolor) end				 
				end					
			end			
		end
	end
	
	local ControlObject = Control.Font
	if ControlObject then
		local tTheme = Theme.Font
		if tTheme then			
			if tTheme.Size then ControlObject:SetFontSize(tTheme.Size) end
			if tTheme.Name then ControlObject:SetFontName(tTheme.Name) end
			if tTheme.Bold then ControlObject:SetBold(tTheme.Bold) end
			if tTheme.Italic then ControlObject:SetItalic(tTheme.Italic) end
			if tTheme.Underline then ControlObject:SetUnderline(tTheme.Underline) end		
		end		
	end	
	
	local ControlObject = Control.BackgroundImage
	if ControlObject then
		local tTheme = Theme.BackgroundImage
		if tTheme then			
			if tTheme.Dependency then
				local Dependency = ModuleDependencys:GetDependency(tTheme.Dependency)
				if Dependency then 
					ControlObject:SetDependency(Dependency)
					ControlObject:SetEnabled(true)
				end
			end
			
			if tTheme.Alpha then ControlObject:SetTransparencyLevel(tTheme.Alpha) end	

		end		
	end		
	
end


Theme = {}

Theme.CheckBox = {
	Font = {Size=GUI_DEFAULTFONTSIZE+0.5, Name=GUI_DEFAULTFONT, Bold=true},
	Color = {Checked_Forecolor=RGBToHex(255,0,0), Unchecked_Forecolor=COLOR_GREEN}
}

Theme.Button = {
	Font = {Size=GUI_DEFAULTFONTSIZE, Name=GUI_DEFAULTFONT, Bold=true},
}

Theme.FixToggle = {
	Font = {Size=GUI_DEFAULTFONTSIZE+0.5, Name=GUI_DEFAULTFONT, Bold=true},
	Color = {Checked_Forecolor=RGBToHex(255,0,0), Unchecked_Forecolor=COLOR_GREEN}
}

Theme.Header = {
	Font = {Size=GUI_DEFAULTFONTSIZE, Name=GUI_DEFAULTFONT, Bold=true},
	Color = {Backcolor=RGBToHex(255,64,64), Forecolor=COLOR_WHITE, Alpha=100}
}

Theme.Warning = {
	Font = {Size=GUI_DEFAULTFONTSIZE, Name=GUI_DEFAULTFONT, Bold=true},
	Color = {Backcolor=RGBToHex(255,32,32), Forecolor=COLOR_WHITE, Alpha=70}
}

Theme.Label = {
	Font = {Size=GUI_DEFAULTFONTSIZE, Name=GUI_DEFAULTFONT},
	Color = {Backcolor=RGBToHex(255,64,64), Forecolor=COLOR_WHITE, Alpha=100}
}

Theme.FOVSlider = {
	Labels = {
		Font = {Size=GUI_DEFAULTFONTSIZE, Name=GUI_DEFAULTFONT, Bold=true},
		Color = {Forecolor=COLOR_WHITE}
	},
	ValueLabel = {
		Font = {Size=GUI_DEFAULTFONTSIZE-0.5, Name=GUI_DEFAULTFONT},
		Color = {Forecolor=COLOR_WHITE}
	}
}

Theme.ComboBox = {
	Font = {Size=GUI_DEFAULTFONTSIZE, Name=GUI_DEFAULTFONT},
	Color = {Forecolor=COLOR_WHITE}
}

Theme.ParameterBox = {
	Font = {Size=GUI_DEFAULTFONTSIZE, Name=GUI_DEFAULTFONT},
	Color = {Backcolor=RGBToHex(50,50,255), Forecolor=COLOR_WHITE, Alpha=40}
}

Theme.ContainerHeader = {
	Font = {Size=GUI_DEFAULTFONTSIZE, Name=GUI_DEFAULTFONT, Bold=true},
	Color = {Backcolor=COLOR_GREEN, Forecolor=COLOR_GREEN, Alpha=60}
}

Theme.ContainerLabelName = {
	Font = {Size=GUI_DEFAULTFONTSIZE-0.25, Name=GUI_DEFAULTFONT, Bold=true},
	Color = {Forecolor=COLOR_WHITE}
}

Theme.ContainerLabelValue = {
	Font = {Size=GUI_DEFAULTFONTSIZE-0.25, Name=GUI_DEFAULTFONT},
	Color = {Forecolor=COLOR_WHITE}
}

Theme.ContainerLinkLabel = {
	Font = {Size=GUI_DEFAULTFONTSIZE-0.5, Name=GUI_DEFAULTFONT},
	Color = {Forecolor=COLOR_WHITE}
}

Theme.ContainerEditBox = {
	Font = {Size=GUI_DEFAULTFONTSIZE-0.5, Name=GUI_DEFAULTFONT},
	Color = {Backcolor=COLOR_WHITE, Forecolor=COLOR_WHITE, Alpha=25}
}

Theme.MainWindow = {
	Color = {Backcolor=COLOR_BLACK},
	BackgroundImage = {Dependency="UI Background", Alpha=45}
}

Theme.Container = {
	Color = {Backcolor=COLOR_WHITE, Alpha=25}
}

--MainWindow
ApplyThemeToControl(MainWindow,Theme.MainWindow)

-- Container Window
ApplyThemeToControl(ContainerChild,Theme.Container)

-- Information Header
ApplyThemeToControl(lbl_InfoHeader,Theme.ContainerHeader)
ApplyThemeToControl(lbl_InfoFilename,Theme.ContainerLabelName)
ApplyThemeToControl(lbl_InfoFilenameVal,Theme.ContainerLabelValue)
ApplyThemeToControl(lbl_Version,Theme.ContainerLabelName)
ApplyThemeToControl(lbl_VersionVal,Theme.ContainerLabelValue)
ApplyThemeToControl(lbl_InfoModType,Theme.ContainerLabelName)
ApplyThemeToControl(lbl_InfoModTypeVal,Theme.ContainerLabelValue)
ApplyThemeToControl(lbl_InfoModName,Theme.ContainerLabelName)
ApplyThemeToControl(lbl_InfoModNameVal,Theme.ContainerLabelValue)
ApplyThemeToControl(lbl_InfoAuthor,Theme.ContainerLabelName)
ApplyThemeToControl(lbl_InfoAuthorVal,Theme.ContainerLabelValue)

--Description
ApplyThemeToControl(lbl_DescrHeader,Theme.ContainerHeader)
ApplyThemeToControl(eb_Description,Theme.ContainerEditBox)

--Links and Resources
ApplyThemeToControl(lbl_LinkResourceHeader,Theme.ContainerHeader)
ApplyThemeToControl(link_LinkBlock,Theme.ContainerLinkLabel)

--Status Window
ApplyThemeToControl(lbl_StatusWindowHeader,Theme.ContainerHeader)
ApplyThemeToControl(StatusWindowEditBox,Theme.ContainerEditBox)