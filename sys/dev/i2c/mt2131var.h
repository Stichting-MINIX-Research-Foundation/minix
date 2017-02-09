
#ifndef _DEV_I2C_MT2131VAR_H_
#define _DEV_I2C_MT2131VAR_H_

#include <dev/i2c/i2cvar.h>
#include <dev/dtv/dtvio.h>

struct mt2131_softc;

struct mt2131_softc * mt2131_open(device_t, i2c_tag_t, i2c_addr_t);
void mt2131_close(struct mt2131_softc *);
int mt2131_tune_dtv(struct mt2131_softc *, const struct dvb_frontend_parameters *);
int mt2131_get_status(struct mt2131_softc *);

#endif /* !_DEV_I2C_MT2131VAR_H_ */
