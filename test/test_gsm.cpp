#include "unity.h"
#include "Gprs.h"


GPRS gprs;

void gprs_send_data()
{
  char terId[8];
  gprs.getTerminalID(terId);
  vTaskDelay(2000 / portTICK_RATE_MS);

  /*Get time from gprs device*/
  char tims[7];
  gprs.getTime(tims);
  /*Get terminal id from gprs device*/
  vTaskDelay(2000 / portTICK_RATE_MS);

  /*START PPP CONNECTION*/
  gprs.start();
  /*WAIT FOR SETTLING THE CONNECTION*/
  char cod_mot[10] = {1,2,3,4,5,6,6,7,7,5};
  char cod_lin[10] = {3,4,5,4,5,6,6,9,2,5};
  gprs.encode_data(tims, terId, cod_mot, cod_lin);

  /*IT'S VERY IMPORTANT CLOSE THE PPP CONNECTION*/
  /*This part could be used in a non-bloking context
     takes at least 30 seconds to close the connection
   */

  gprs.stop();
}


TEST_CASE("test_gprs", "[GPRS]",
{
        gprs_send_data
}
