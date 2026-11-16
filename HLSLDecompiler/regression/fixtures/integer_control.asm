// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float   xyzw
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyzw        0   TARGET   float   xyzw
//
ps_5_0
dcl_globalFlags refactoringAllowed
dcl_resource_texture2d (uint,uint,uint,uint) t10
dcl_resource_texture2d (sint,sint,sint,sint) t11
dcl_input_ps_siv linear noperspective v0.xyzw, position
dcl_output o0.xyzw
dcl_temps 3
mov r0.xyzw, l(0,0,0,0)
ld r1.xyzw, r0.xyzw, t10.xyzw
ld r2.z, r0.xyzw, t10.xxxx
and r2.x, r2.z, l(15)
switch r2.x
case l(1)
mov o0.x, l(11.000000)
break
case l(15)
mov o0.x, l(15.000000)
break
default
mov o0.x, l(99.000000)
break
endswitch
if_nz r1.y
mov o0.y, l(21.000000)
else
mov o0.y, l(22.000000)
endif
mov o0.z, r2.z
ld r2.w, r0.xyzw, t11.zzzz
mov o0.w, r2.w
ret
