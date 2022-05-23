/**********************************************************************************/
/* CREATE RESTORE POINT command                                                   */
/* Usage:                                                                         */
/*    create restore point                                                        */                 
/*      Available Option(s):                                                      */
/*         /temporary                 Create a temporary restore point            */
/*         /permanent                 Create a permanent restore point            */
/*      Available Qualifier(s):                                                   */
/*         /name=<String>             Restore point name                          */
/**********************************************************************************/
void COMMAND_CREATERP(int idcmd,char *command_line)
{
   if (isConnectedAndRegistered() == false) return;
   
   if (optionIsSET("opt_verbose") == true) globalArgs.verbosity=true;                                                              // Set Verbosity
   
   memBeginModule();

   char *query=memAlloc(1024);
   char *restore_point_type=memAlloc(10);
   strcpy(restore_point_type,"T");
   
   if (optionIsUNSET("opt_temporary") == true && optionIsUNSET("opt_permanent") == true)                                           // Set TEMPORARY restore point by default
   {
      SETOption("opt_temporary");
   }
   
   if (optionIsUNSET("opt_temporary") == true && optionIsUNSET("opt_permanent") == true)                                           // Accept only one option at a time
   {
      ERROR(ERR_BADOPTION,"Please, choose between '/temporary' or '/permanent'. Both are not accepted.\n");
      memEndModule();
      return;
   }
   if (optionIsSET("opt_temporary") == true) { strcpy(restore_point_type,"T"); };
   if (optionIsSET("opt_permanent") == true) { strcpy(restore_point_type,"P"); };

   if (qualifierIsUNSET("qal_name") == true)
   {
      ERROR(ERR_MISQUALVAL,"Missing /name=xxx qualifier.\n");
      memEndModule();
      return;
   };
   if (isValidName(varGet("qal_name")) == false)                                                                                   // Reject some characters
   {
      ERROR(ERR_INVCHARINNAME,"Invalid character in name '%s'\n",varGet("qal_name"));
      memEndModule();
      return;
   }
   sprintf(query,"select count(*) from %s.backup_rp where cid=%s and lower(rp_name) = lower('%s')",                                // Verify if restore point already exist
                 varGet(GVAR_DEPUSER),
                 varGet(GVAR_CLUCID),
                 varGet("qal_name"));
   int row=DEPOquery(query,0);
   if (DEPOgetInt(0,0) != 0)
   {
      ERROR(ERR_NAMEEXIST,"Restore point name '%s' already exist.\n",varGet("qal_name"));
      DEPOqueryEnd();
      memEndModule();
      return;
   }
   DEPOqueryEnd();
   
   sprintf(query,"select pg_create_restore_point('%s');",varGet("qal_name"));                                                      // Create restore point in PostgreSQL
   row=CLUquery(query,0);
   if (row != 1)
   {
      ERROR(ERR_CREATRPFAILED,"Could not create restore point '%s'\n",varGet("qal_name"));
      CLUqueryEnd();
      memEndModule();
      return;
   }
   char *lsn=memAlloc(128);
   char *walfile=memAlloc(128);
   strcpy(lsn,CLUgetString(0,0));
   CLUqueryEnd();
      
   sprintf(query,"select pg_walfile_name('%s');",lsn);                                                                             // Get Current WAL file
   row=CLUquery(query,0);
   if (row != 1)
   {
      ERROR(ERR_CREATRPFAILED,"Could not create restore point '%s'\n",varGet("qal_name"));
      memEndModule();
      return;
   }
   strcpy(walfile,CLUgetString(0,0));
   CLUqueryEnd();
   
   cluster_doSwitchWAL(true);                                                                                                      // Force a switch WAL


   VERBOSE("Restore point '%s' created at '%s' (WAL %s)\n",varGet("qal_name"),lsn,walfile);

   sprintf(query,"insert into %s.backup_rp(cid,rp_name,lsn,tl,wal,rp_typ) values(%s,lower('%s'),'%s',%ld,'%s','%s')",              // Save restore point in DEPOSIT
                 varGet(GVAR_DEPUSER),
                 varGet(GVAR_CLUCID),
                 varGet("qal_name"),
                 lsn,
                 pgGetCurrentTimeline(),
                 walfile,
                 restore_point_type);
   int rc=DEPOquery(query,0);
   TRACE("[rc=%d]\n",rc);
   DEPOqueryEnd();
   memEndModule();
   return;
};

   