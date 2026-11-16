
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_POSITION              0   xyzw        0      POS   float       
// TEXCOORD                 0   xyzw        1     NONE   float   xy  
// TEXCOORD                 1   xyz         2     NONE   float   xyz 
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_Target                0   xyz         0   TARGET   float   xyz 
// SV_Target                1   xyz         1   TARGET   float   xyz 
//
ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer cb0[114], immediateIndexed
      dcl_constantbuffer cb1[9], immediateIndexed
      dcl_constantbuffer cb2[46], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_sampler s1, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_resource_texture2d (float,float,float,float) t1
      dcl_resource_texture2d (float,float,float,float) t2
      dcl_resource_texturecube (float,float,float,float) t3
      dcl_resource_texturecubearray (float,float,float,float) t4
      dcl_resource_structured t5, 16
      dcl_resource_structured t6, 112
      dcl_resource_structured t7, 8
      dcl_resource_structured t8, 4
      dcl_resource_texture2d (float,float,float,float) t9
      dcl_input_ps linear v1.xy
      dcl_input_ps linear v2.xyz
      dcl_output o0.xyz
      dcl_output o1.xyz
      dcl_temps 22
mul r0.xy, v1.xyxx, cb1[7].xyxx
ftoi r0.xy, r0.xyxx
mov r0.zw, l(0, 0, 0, 0)
ld_indexable(texture2d)(float,float,float,float) r1.xyz, r0.xyww, t1.xyzw
mad r1.xyz, r1.xyzx, l(2.000000, 2.000000, 2.000000, 0.000000), l(-1.000000, -1.000000, -1.000000, 0.000000)
dp3 r1.w, r1.xyzx, r1.xyzx
rsq r1.w, r1.w
mul r1.xyz, r1.wwww, r1.xyzx
ld_indexable(texture2d)(float,float,float,float) r0.x, r0.xyzw, t2.xyzw
sample_indexable(texture2d)(float,float,float,float) r0.y, v1.xyxx, t0.yxzw, s1
mad r0.yz, cb1[8].xxzx, r0.yyyy, cb1[8].yywy
div r0.yz, l(1.000000, 1.000000, 1.000000, 1.000000), r0.yyzy
mad r2.xyz, v2.xyzx, r0.yyyy, cb1[5].xyzx
dp3 r0.y, v2.xyzx, v2.xyzx
rsq r0.y, r0.y
mul r3.xyz, r0.yyyy, v2.xyzx
add r0.x, -r0.x, l(1.000000)
dp3 r0.y, r3.xyzx, r1.xyzx
add r0.y, r0.y, r0.y
mad r3.xyz, r1.xyzx, -r0.yyyy, r3.xyzx
resinfo_indexable(texture2d)(float,float,float,float)_uint r0.y, l(0), t9.xwyz
ine r0.w, r0.y, l(1)
if_nz r0.w
mul r0.w, r0.x, r0.x
iadd r0.y, r0.y, l(-4)
mad r2.w, -cb0[109].y, l(4.000000), l(2.450000)
log r0.w, r0.w
mul r0.w, r0.w, r2.w
exp r0.w, r0.w
utof r0.y, r0.y
mul r0.w, r0.y, r0.w
min r0.y, r0.y, r0.w
mul r4.xy, v1.xyxx, cb0[111].xyxx
min r4.xy, r4.xyxx, cb0[111].zwzz
sample_l r4.xyzw, r4.xyxx, t9.xyzw, s0, r0.y
mul r4.w, r4.w, cb0[110].x
else
mov r4.xyzw, l(0, 0, 0, 0)
endif
mul r0.yw, v1.xxxy, l(0.000000, 32.000000, 0.000000, 24.000000)
round_ni r0.yw, r0.yyyw
ftoi r0.yw, r0.yyyw
mul r0.z, r0.z, l(0.029297)
round_ni r0.z, r0.z
ftoi r0.z, r0.z
imin r0.yzw, r0.yyzw, l(0, 31, 15, 23)
ishl r0.w, r0.w, l(5)
iadd r0.y, r0.y, r0.w
ishl r0.y, r0.y, l(4)
iadd r0.y, r0.z, r0.y
ld_structured_indexable(structured_buffer, stride=8)(mixed,mixed,mixed,mixed) r0.yz, r0.y, l(0), t7.xxyx
ge r0.w, l(0), cb0[112].x
eq r2.w, cb0[113].x, l(0)
mul r5.xyzw, r1.yzzx, r1.xyzz
mul r3.w, r1.y, r1.y
mad r3.w, r1.x, r1.x, -r3.w
mul r6.x, r0.x, l(6.000000)
dp3 r6.y, r3.xyzx, r3.xyzx
rsq r6.y, r6.y
mul r6.yzw, r3.xxyz, r6.yyyy
mov r1.w, l(1.000000)
mov r7.xyzw, r4.xyzw
mov r8.xyz, l(0, 0, 0, 0)
mov r8.w, r0.y
mov r9.xy, l(0, 0, 0, 0)
loop
ine r9.z, r8.w, l(0)
lt r9.w, r9.x, l(1.000000)
and r9.z, r9.w, r9.z
breakc_z r9.z
firstbit_lo r9.z, r8.w
ishl r9.w, l(1), r9.z
not r9.w, r9.w
and r8.w, r8.w, r9.w
ld_structured_indexable(structured_buffer, stride=4)(mixed,mixed,mixed,mixed) r9.z, r9.z, l(0), t8.xxxx
ld_structured_indexable(structured_buffer, stride=112)(mixed,mixed,mixed,mixed) r10.xy, r9.z, l(96), t6.xyxx
lt r9.w, l(0), r10.y
and r9.w, r0.w, r9.w
if_z r9.w
ld_structured_indexable(structured_buffer, stride=112)(mixed,mixed,mixed,mixed) r11.xyzw, r9.z, l(0), t6.xyzw
ld_structured_indexable(structured_buffer, stride=112)(mixed,mixed,mixed,mixed) r12.xyzw, r9.z, l(16), t6.xyzw
ld_structured_indexable(structured_buffer, stride=112)(mixed,mixed,mixed,mixed) r13.xyzw, r9.z, l(32), t6.yxwz
ld_structured_indexable(structured_buffer, stride=112)(mixed,mixed,mixed,mixed) r14.xyzw, r9.z, l(48), t6.xyzw
ld_structured_indexable(structured_buffer, stride=112)(mixed,mixed,mixed,mixed) r15.xyzw, r9.z, l(64), t6.xyzw
add r10.yzw, r2.xxyz, -r11.xxyz
mov r16.x, r14.w
mov r16.y, r12.z
mov r16.z, r13.x
dp3 r17.x, r16.xyzx, r10.yzwy
mov r18.xy, r12.xwxx
mov r18.z, r13.w
dp3 r17.y, r18.xyzx, r10.yzwy
mov r13.x, r12.y
dp3 r17.z, r13.xyzx, r10.yzwy
add r11.xyz, r11.xyzx, r17.xyzx
add r12.xyz, -r15.xyzx, r11.xyzx
add r17.xyz, -r11.xyzx, r14.xyzx
max r12.xyz, r12.xyzx, r17.xyzx
max r12.xyz, r12.xyzx, l(0, 0, 0, 0)
dp3 r9.w, r12.xyzx, r12.xyzx
sqrt r9.w, r9.w
div r9.w, r9.w, r15.w
add_sat r9.w, -r9.w, l(1.000000)
lt r12.x, l(0), r9.w
if_nz r12.x
ld_structured_indexable(structured_buffer, stride=112)(mixed,mixed,mixed,mixed) r12.xy, r9.z, l(80), t6.xyxx
if_nz r2.w
add r12.z, -r9.x, l(1.000000)
min r12.z, r9.w, r12.z
imul null, r12.w, r9.z, l(7)
ld_structured_indexable(structured_buffer, stride=16)(mixed,mixed,mixed,mixed) r17.xyzw, r12.w, l(0), t5.xyzw
dp4 r12.w, r17.xyzw, r1.xyzw
imad r17.xyzw, r9.zzzz, l(7, 7, 7, 7), l(6, 1, 2, 3)
ld_structured_indexable(structured_buffer, stride=16)(mixed,mixed,mixed,mixed) r19.xyzw, r17.y, l(0), t5.xyzw
dp4 r13.w, r19.xyzw, r1.xyzw
ld_structured_indexable(structured_buffer, stride=16)(mixed,mixed,mixed,mixed) r19.xyzw, r17.z, l(0), t5.xyzw
dp4 r14.w, r19.xyzw, r1.xyzw
ld_structured_indexable(structured_buffer, stride=16)(mixed,mixed,mixed,mixed) r19.xyzw, r17.w, l(0), t5.xyzw
dp4 r16.w, r19.xyzw, r5.xyzw
add r19.x, r12.w, r16.w
imad r17.yz, r9.zzzz, l(0, 7, 7, 0), l(0, 4, 5, 0)
ld_structured_indexable(structured_buffer, stride=16)(mixed,mixed,mixed,mixed) r20.xyzw, r17.y, l(0), t5.xyzw
dp4 r12.w, r20.xyzw, r5.xyzw
add r19.y, r12.w, r13.w
ld_structured_indexable(structured_buffer, stride=16)(mixed,mixed,mixed,mixed) r20.xyzw, r17.z, l(0), t5.xyzw
dp4 r12.w, r20.xyzw, r5.xyzw
add r19.z, r12.w, r14.w
ld_structured_indexable(structured_buffer, stride=16)(mixed,mixed,mixed,mixed) r17.xyzw, r17.x, l(0), t5.xyzw
mad r17.xyz, r17.xyzx, r3.wwww, r19.xyzx
max r17.xyz, r17.xyzx, l(0, 0, 0, 0)
mul r17.xyz, r12.zzzz, r17.xyzx
mul r17.xyz, r12.yyyy, r17.xyzx
mad r8.xyz, r17.xyzx, r17.wwww, r8.xyzx
mad r9.y, r12.z, r17.w, r9.y
add r9.x, r9.x, r12.z
else
add r12.y, -r9.x, l(1.000000)
min r12.y, r9.w, r12.y
add r9.x, r9.x, r12.y
mov r9.y, l(1.000000)
endif
lt r12.y, r7.w, l(1.000000)
if_nz r12.y
mad r10.x, r0.x, l(6.000000), r10.x
lt r11.w, l(0), r11.w
if_nz r11.w
add r12.yzw, r14.xxyz, -r15.wwww
add r14.xyz, r15.wwww, r15.xyzx
dp3 r15.x, r16.xyzx, r6.yzwy
dp3 r15.y, r18.xyzx, r6.yzwy
dp3 r15.z, r13.xyzx, r6.yzwy
add r13.xyz, -r11.xyzx, r14.xyzx
div r13.xyz, r13.xyzx, r15.xyzx
add r11.xyz, -r11.xyzx, r12.yzwy
div r11.xyz, r11.xyzx, r15.xyzx
lt r12.yzw, l(0, 0, 0, 0), r15.xxyz
movc r11.xyz, r12.yzwy, r13.xyzx, r11.xyzx
min r11.x, r11.y, r11.x
min r11.x, r11.z, r11.x
mad r11.xyz, r6.yzwy, r11.xxxx, r10.yzwy
else
mov r11.xyz, r3.xyzx
endif
utof r11.w, r9.z
sample_l r10.xyz, r11.xyzw, t4.xyzw, s0, r10.x
mul r10.xyz, r12.xxxx, r10.xyzx
add r9.z, -r7.w, l(1.000000)
min r9.z, r9.z, r9.w
mad r7.xyz, r10.xyzx, r9.zzzz, r7.xyzx
add r7.w, r7.w, r9.w
endif
endif
endif
endloop
mov r1.w, l(1.000000)
mov r4.xyzw, r7.xyzw
mov r10.xyz, r8.xyzx
mov r0.y, r0.z
mov r8.w, r9.x
mov r9.z, r9.y
loop
ine r9.w, r0.y, l(0)
lt r10.w, r8.w, l(1.000000)
and r9.w, r9.w, r10.w
breakc_z r9.w
firstbit_lo r9.w, r0.y
ishl r10.w, l(1), r9.w
not r10.w, r10.w
and r0.y, r0.y, r10.w
iadd r9.w, r9.w, l(32)
ld_structured_indexable(structured_buffer, stride=4)(mixed,mixed,mixed,mixed) r9.w, r9.w, l(0), t8.xxxx
ld_structured_indexable(structured_buffer, stride=112)(mixed,mixed,mixed,mixed) r11.xy, r9.w, l(96), t6.xyxx
lt r10.w, l(0), r11.y
and r10.w, r0.w, r10.w
if_z r10.w
ld_structured_indexable(structured_buffer, stride=112)(mixed,mixed,mixed,mixed) r12.xyzw, r9.w, l(0), t6.xyzw
ld_structured_indexable(structured_buffer, stride=112)(mixed,mixed,mixed,mixed) r13.xyzw, r9.w, l(16), t6.xyzw
ld_structured_indexable(structured_buffer, stride=112)(mixed,mixed,mixed,mixed) r14.xyzw, r9.w, l(32), t6.yxwz
ld_structured_indexable(structured_buffer, stride=112)(mixed,mixed,mixed,mixed) r15.xyzw, r9.w, l(48), t6.xyzw
ld_structured_indexable(structured_buffer, stride=112)(mixed,mixed,mixed,mixed) r16.xyzw, r9.w, l(64), t6.xyzw
add r11.yzw, r2.xxyz, -r12.xxyz
mov r17.x, r15.w
mov r17.y, r13.z
mov r17.z, r14.x
dp3 r18.x, r17.xyzx, r11.yzwy
mov r19.xy, r13.xwxx
mov r19.z, r14.w
dp3 r18.y, r19.xyzx, r11.yzwy
mov r14.x, r13.y
dp3 r18.z, r14.xyzx, r11.yzwy
add r12.xyz, r12.xyzx, r18.xyzx
add r13.xyz, -r16.xyzx, r12.xyzx
add r18.xyz, -r12.xyzx, r15.xyzx
max r13.xyz, r13.xyzx, r18.xyzx
max r13.xyz, r13.xyzx, l(0, 0, 0, 0)
dp3 r10.w, r13.xyzx, r13.xyzx
sqrt r10.w, r10.w
div r10.w, r10.w, r16.w
add_sat r10.w, -r10.w, l(1.000000)
lt r13.x, l(0), r10.w
if_nz r13.x
ld_structured_indexable(structured_buffer, stride=112)(mixed,mixed,mixed,mixed) r13.xy, r9.w, l(80), t6.xyxx
if_nz r2.w
add r13.z, -r8.w, l(1.000000)
min r13.z, r10.w, r13.z
imul null, r13.w, r9.w, l(7)
ld_structured_indexable(structured_buffer, stride=16)(mixed,mixed,mixed,mixed) r18.xyzw, r13.w, l(0), t5.xyzw
dp4 r13.w, r18.xyzw, r1.xyzw
imad r18.xyzw, r9.wwww, l(7, 7, 7, 7), l(6, 1, 2, 3)
ld_structured_indexable(structured_buffer, stride=16)(mixed,mixed,mixed,mixed) r20.xyzw, r18.y, l(0), t5.xyzw
dp4 r14.w, r20.xyzw, r1.xyzw
ld_structured_indexable(structured_buffer, stride=16)(mixed,mixed,mixed,mixed) r20.xyzw, r18.z, l(0), t5.xyzw
dp4 r15.w, r20.xyzw, r1.xyzw
ld_structured_indexable(structured_buffer, stride=16)(mixed,mixed,mixed,mixed) r20.xyzw, r18.w, l(0), t5.xyzw
dp4 r17.w, r20.xyzw, r5.xyzw
add r20.x, r13.w, r17.w
imad r18.yz, r9.wwww, l(0, 7, 7, 0), l(0, 4, 5, 0)
ld_structured_indexable(structured_buffer, stride=16)(mixed,mixed,mixed,mixed) r21.xyzw, r18.y, l(0), t5.xyzw
dp4 r13.w, r21.xyzw, r5.xyzw
add r20.y, r13.w, r14.w
ld_structured_indexable(structured_buffer, stride=16)(mixed,mixed,mixed,mixed) r21.xyzw, r18.z, l(0), t5.xyzw
dp4 r13.w, r21.xyzw, r5.xyzw
add r20.z, r13.w, r15.w
ld_structured_indexable(structured_buffer, stride=16)(mixed,mixed,mixed,mixed) r18.xyzw, r18.x, l(0), t5.xyzw
mad r18.xyz, r18.xyzx, r3.wwww, r20.xyzx
max r18.xyz, r18.xyzx, l(0, 0, 0, 0)
mul r18.xyz, r13.zzzz, r18.xyzx
mul r18.xyz, r13.yyyy, r18.xyzx
mad r10.xyz, r18.xyzx, r18.wwww, r10.xyzx
mad r9.z, r13.z, r18.w, r9.z
add r8.w, r8.w, r13.z
else
add r13.y, -r8.w, l(1.000000)
min r13.y, r10.w, r13.y
add r8.w, r8.w, r13.y
mov r9.z, l(1.000000)
endif
lt r13.y, r4.w, l(1.000000)
if_nz r13.y
mad r11.x, r0.x, l(6.000000), r11.x
lt r12.w, l(0), r12.w
if_nz r12.w
add r13.yzw, r15.xxyz, -r16.wwww
add r15.xyz, r16.wwww, r16.xyzx
dp3 r16.x, r17.xyzx, r6.yzwy
dp3 r16.y, r19.xyzx, r6.yzwy
dp3 r16.z, r14.xyzx, r6.yzwy
add r14.xyz, -r12.xyzx, r15.xyzx
div r14.xyz, r14.xyzx, r16.xyzx
add r12.xyz, -r12.xyzx, r13.yzwy
div r12.xyz, r12.xyzx, r16.xyzx
lt r13.yzw, l(0, 0, 0, 0), r16.xxyz
movc r12.xyz, r13.yzwy, r14.xyzx, r12.xyzx
min r12.x, r12.y, r12.x
min r12.x, r12.z, r12.x
mad r12.xyz, r6.yzwy, r12.xxxx, r11.yzwy
else
mov r12.xyz, r3.xyzx
endif
utof r12.w, r9.w
sample_l r11.xyz, r12.xyzw, t4.xyzw, s0, r11.x
mul r11.xyz, r13.xxxx, r11.xyzx
add r9.w, -r4.w, l(1.000000)
min r9.w, r9.w, r10.w
mad r4.xyz, r11.xyzx, r9.wwww, r4.xyzx
add r4.w, r4.w, r10.w
endif
endif
endif
endloop
mov r1.w, l(1.000000)
dp4 r0.x, cb2[39].xyzw, r1.xyzw
dp4 r0.y, cb2[40].xyzw, r1.xyzw
dp4 r0.z, cb2[41].xyzw, r1.xyzw
dp4 r1.x, cb2[42].xyzw, r5.xyzw
dp4 r1.y, cb2[43].xyzw, r5.xyzw
dp4 r1.z, cb2[44].xyzw, r5.xyzw
mad r1.xyz, cb2[45].xyzx, r3.wwww, r1.xyzx
add r0.xyz, r0.xyzx, r1.xyzx
max r0.xyz, r0.xyzx, l(0, 0, 0, 0)
mad r0.w, -r8.w, r9.z, l(1.000000)
mad o1.xyz, r0.xyzx, r0.wwww, r10.xyzx
lt r0.x, r4.w, l(1.000000)
if_nz r0.x
sample_l r0.xyz, r3.xyzx, t3.xyzw, s0, r6.x
mul r0.xyz, r0.xyzx, cb0[107].xxxx
add r0.w, -r4.w, l(1.000000)
mad o0.xyz, r0.xyzx, r0.wwww, r4.xyzx
else
mov o0.xyz, r4.xyzx
endif
ret
