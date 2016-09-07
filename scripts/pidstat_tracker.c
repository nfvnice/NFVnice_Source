#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
  const char *cmd = "pidstat -C \"bridge|forward\" -lrsuwh 2 5 | tee pidstat_log.csv";
  //const char *cmd2 = "perf stat --cpu=8,10 --per-core -r 5 -o pidstat2_log.csv sleep 2"; //-I 2000
  const char *cmd2 = "perf stat --cpu=8,10 --per-core -o pidstat2_log.csv sleep 10"; //-I 2000
/* ls -al | grep '^d' */
  FILE *pp=NULL, *pp2=NULL;
  //pp = popen("ls -al", "r");
  pp = popen(cmd, "r");
  pp2 = popen(cmd2, "r");
  int done = 0;
  if (pp != NULL && pp2 !=NULL) {
    while (1) {
      char *line;
      char buf[1000];
      line = fgets(buf, sizeof buf, pp);
      
      if (line == NULL) done |= 0x01; //break;
      //if (line[0] == 'd') printf("%s", line); // line includes '\n'
        //printf("%s", line);
      line = fgets(buf, sizeof buf, pp2);
      if (line == NULL) done |= 0x02; //break;
      //if (line[0] == 'd') printf("%s", line); // line includes '\n'
        //printf("%s", line);
      if(done == 0x03) break;
    }
    pclose(pp);
    pclose(pp2);
  }
  return 0;
}
