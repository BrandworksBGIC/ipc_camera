test -n ${BOOT_ORDER} || setenv BOOT_ORDER A B;
test -n ${BOOT_A_LEFT} || setenv BOOT_A_LEFT 3;
test -n ${BOOT_B_LEFT} || setenv BOOT_B_LEFT 3;

last_slot_part='';
slot_part='';

for x in ${bootargs_db}; do
  if test ${x} = \'rauc.slot=A\'; then
    last_slot_part=\'A\';
  elif test ${x} = \'rauc.slot=B\'; then
    last_slot_part=\'B\';
  fi;
done;

for BOOT_SLOT in ${BOOT_ORDER}; do
  if test -n ${slot_part}; then
    echo BOOT SLOT=${slot_part};
  elif test x${BOOT_SLOT} = xA; then
    slot_part=\'A\';
    if test x${last_slot_part} != xA; then
      setenv loadaddr_kernel ${kerneladdr_a};
      setenv bootargs_db ${bootargs_a};
      saveenv;
    fi;
  elif test x${BOOT_SLOT} = xB; then
    slot_part=\'B\';
    if test x${last_slot_part} != xB; then
      setenv loadaddr_kernel ${kerneladdr_b};
      setenv bootargs_db ${bootargs_b};
      saveenv;
    fi;
  fi;
done;

if test -n ${bootargs_db}; then
  echo loadaddr_kernel:${loadaddr_kernel};
  echo BOOT_ORDER:${BOOT_ORDER};
else
  echo No valid slot found;
  re;
fi;

echo Loading kernel;
echo Starting kernel;
bootm ${loadaddr_kernel};
