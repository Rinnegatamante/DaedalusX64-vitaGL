  .set noreorder

  .global _start
  .ent    _start
_start:
  mfc0  $k0, $12
  lui   $k1, 0xffbf
  ori   $k1, $k1, 0xfff9
  and   $k0, $k0, $k1
  li    $k1, 0
  or    $k0, $k0, $k1
  mtc0  $k0, $12
  move  $k1, $0
  lui   $sp, 0x8880
  addiu $sp, $sp, -64
  la    $v0, main
  jr    $v0
  nop

  move  $v0, $a0
  addu  $t0, $a0, $a1
  addiu $t0, $t0, -4

loc_88600044:
  sw    $0, 0($a0)
  bne   $t0, $a0, loc_88600044
  addiu $a0, $a0, 4

loc_88600050:
  sw    $0, 0($v0)
  bne   $t0, $v0, loc_88600050
  addiu $v0, $v0, 4

  break
.end _start
