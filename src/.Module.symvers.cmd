cmd_/home/alessandrodea/Scrivania/SOAproject/src/Module.symvers := sed 's/\.ko$$/\.o/' /home/alessandrodea/Scrivania/SOAproject/src/modules.order | scripts/mod/modpost  -a  -o /home/alessandrodea/Scrivania/SOAproject/src/Module.symvers -e -i Module.symvers  -N -T -
