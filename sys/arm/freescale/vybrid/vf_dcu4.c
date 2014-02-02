/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Vybrid Family Display Control Unit (DCU4)
 * Chapter 55, Vybrid Reference Manual, Rev. 5, 07/2013
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/watchdog.h>
#include <sys/fbio.h>
#include <sys/consio.h>
#include <sys/eventhandler.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/vt/vt.h>
#include <dev/vt/colors/vt_termcolors.h>

#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include "fb_if.h"

#include <arm/freescale/vybrid/vf_common.h>

#define	DCU_CTRLDESCCURSOR1	0x000	/* Control Descriptor Cursor 1 */
#define	DCU_CTRLDESCCURSOR2	0x004	/* Control Descriptor Cursor 2 */
#define	DCU_CTRLDESCCURSOR3	0x008	/* Control Descriptor Cursor 3 */
#define	DCU_CTRLDESCCURSOR4	0x00C	/* Control Descriptor Cursor 4 */
#define	DCU_DCU_MODE		0x010	/* DCU4 Mode */
#define	 DCU_MODE_M		0x3
#define	 DCU_MODE_S		0
#define	 DCU_MODE_NORMAL	0x1
#define	 DCU_MODE_TEST		0x2
#define	 DCU_MODE_COLBAR	0x3
#define	 RASTER_EN		(1 << 14)	/* Raster scan of pixel data */
#define	DCU_BGND		0x014	/* Background */
#define	DCU_DISP_SIZE		0x018	/* Display Size */
#define	 DELTA_M		0x7ff
#define	 DELTA_Y_S		16
#define	 DELTA_X_S		0
#define	DCU_HSYN_PARA		0x01C	/* Horizontal Sync Parameter */
#define	 BP_H_SHIFT		22
#define	 PW_H_SHIFT		11
#define	 FP_H_SHIFT		0
#define	DCU_VSYN_PARA		0x020	/* Vertical Sync Parameter */
#define	 BP_V_SHIFT		22
#define	 PW_V_SHIFT		11
#define	 FP_V_SHIFT		0
#define	DCU_SYNPOL		0x024	/* Synchronize Polarity */
#define	 INV_HS			(1 << 0)
#define	 INV_VS			(1 << 1)
#define	DCU_THRESHOLD		0x028	/* Threshold */
#define	 LS_BF_VS_SHIFT		16
#define	 OUT_BUF_HIGH_SHIFT	8
#define	 OUT_BUF_LOW_SHIFT	0
#define	DCU_INT_STATUS		0x02C	/* Interrupt Status */
#define	DCU_INT_MASK		0x030	/* Interrupt Mask */
#define	DCU_COLBAR_1		0x034	/* COLBAR_1 */
#define	DCU_COLBAR_2		0x038	/* COLBAR_2 */
#define	DCU_COLBAR_3		0x03C	/* COLBAR_3 */
#define	DCU_COLBAR_4		0x040	/* COLBAR_4 */
#define	DCU_COLBAR_5		0x044	/* COLBAR_5 */
#define	DCU_COLBAR_6		0x048	/* COLBAR_6 */
#define	DCU_COLBAR_7		0x04C	/* COLBAR_7 */
#define	DCU_COLBAR_8		0x050	/* COLBAR_8 */
#define	DCU_DIV_RATIO		0x054	/* Divide Ratio */
#define	DCU_SIGN_CALC_1		0x058	/* Sign Calculation 1 */
#define	DCU_SIGN_CALC_2		0x05C	/* Sign Calculation 2 */
#define	DCU_CRC_VAL		0x060	/* CRC Value */
#define	DCU_PDI_STATUS		0x064	/* PDI Status */
#define	DCU_PDI_STA_MSK		0x068	/* PDI Status Mask */
#define	DCU_PARR_ERR_STATUS1	0x06C	/* Parameter Error Status 1 */
#define	DCU_PARR_ERR_STATUS2	0x070	/* Parameter Error Status 2 */
#define	DCU_PARR_ERR_STATUS3	0x07C	/* Parameter Error Status 3 */
#define	DCU_MASK_PARR_ERR_ST1	0x080	/* Mask Parameter Error Status 1 */
#define	DCU_MASK_PARR_ERR_ST2	0x084	/* Mask Parameter Error Status 2 */
#define	DCU_MASK_PARR_ERR_ST3	0x090	/* Mask Parameter Error Status 3 */
#define	DCU_THRESHOLD_INP_BUF_1	0x094	/* Threshold Input 1 */
#define	DCU_THRESHOLD_INP_BUF_2	0x098	/* Threshold Input 2 */
#define	DCU_THRESHOLD_INP_BUF_3	0x09C	/* Threshold Input 3 */
#define	DCU_LUMA_COMP		0x0A0	/* LUMA Component */
#define	DCU_CHROMA_RED		0x0A4	/* Red Chroma Components */
#define	DCU_CHROMA_GREEN	0x0A8	/* Green Chroma Components */
#define	DCU_CHROMA_BLUE		0x0AC	/* Blue Chroma Components */
#define	DCU_CRC_POS		0x0B0	/* CRC Position */
#define	DCU_LYR_INTPOL_EN	0x0B4	/* Layer Interpolation Enable */
#define	DCU_LYR_LUMA_COMP	0x0B8	/* Layer Luminance Component */
#define	DCU_LYR_CHRM_RED	0x0BC	/* Layer Chroma Red */
#define	DCU_LYR_CHRM_GRN	0x0C0	/* Layer Chroma Green */
#define	DCU_LYR_CHRM_BLUE	0x0C4	/* Layer Chroma Blue */
#define	DCU_COMP_IMSIZE		0x0C8	/* Compression Image Size */
#define	DCU_UPDATE_MODE		0x0CC	/* Update Mode */
#define	 READREG		(1 << 30)
#define	 MODE			(1 << 31)
#define	DCU_UNDERRUN		0x0D0	/* Underrun */
#define	DCU_GLBL_PROTECT	0x100	/* Global Protection */
#define	DCU_SFT_LCK_BIT_L0	0x104	/* Soft Lock Bit Layer 0 */
#define	DCU_SFT_LCK_BIT_L1	0x108	/* Soft Lock Bit Layer 1 */
#define	DCU_SFT_LCK_DISP_SIZE	0x10C	/* Soft Lock Display Size */
#define	DCU_SFT_LCK_HS_VS_PARA	0x110	/* Soft Lock Hsync/Vsync Parameter */
#define	DCU_SFT_LCK_POL		0x114	/* Soft Lock POL */
#define	DCU_SFT_LCK_L0_TRANSP	0x118	/* Soft Lock L0 Transparency */
#define	DCU_SFT_LCK_L1_TRANSP	0x11C	/* Soft Lock L1 Transparency */

/* Control Descriptor */
#define DCU_CTRLDESCL(n, m)	0x200 + (0x40 * n) + 0x4 * (m - 1)
#define DCU_CTRLDESCLn_1(n)	DCU_CTRLDESCL(n, 1)
#define DCU_CTRLDESCLn_2(n)	DCU_CTRLDESCL(n, 2)
#define DCU_CTRLDESCLn_3(n)	DCU_CTRLDESCL(n, 3)
#define	 TRANS_SHIFT		20
#define DCU_CTRLDESCLn_4(n)	DCU_CTRLDESCL(n, 4)
#define	 BPP_MASK		0xf		/* Bit per pixel Mask */
#define	 BPP_SHIFT		16		/* Bit per pixel Shift */
#define	 BPP24			0x5
#define	 EN_LAYER		(1 << 31)	/* Enable the layer */
#define DCU_CTRLDESCLn_5(n)	DCU_CTRLDESCL(n, 5)
#define DCU_CTRLDESCLn_6(n)	DCU_CTRLDESCL(n, 6)
#define DCU_CTRLDESCLn_7(n)	DCU_CTRLDESCL(n, 7)
#define DCU_CTRLDESCLn_8(n)	DCU_CTRLDESCL(n, 8)
#define DCU_CTRLDESCLn_9(n)	DCU_CTRLDESCL(n, 9)

#define DISPLAY_WIDTH		480
#define DISPLAY_HEIGHT		272

struct dcu_softc {
	struct resource		*res[2];
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	void			*ih;
	device_t		dev;
	device_t		sc_fbd;		/* fbd child */
	struct fb_info		sc_info;
};

static struct resource_spec dcu_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

static int
dcu_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "fsl,mvf600-dcu4"))
		return (ENXIO);

	device_set_desc(dev, "Vybrid Family Display Control Unit (DCU4)");
	return (BUS_PROBE_DEFAULT);
}

static void
dcu_intr(void *arg)
{
	struct dcu_softc *sc;
	int reg;

	sc = arg;

	/* Ack interrupts */
	reg = READ4(sc, DCU_INT_STATUS);
	WRITE4(sc, DCU_INT_STATUS, reg);

	/* TODO interrupt handler */
}

static int
dcu_init(struct dcu_softc *sc)
{
	int reg;

	/* Configure DCU */
	reg = ((sc->sc_info.fb_height) << DELTA_Y_S);
	reg |= (sc->sc_info.fb_width / 16);
	WRITE4(sc, DCU_DISP_SIZE, reg);

	/* TODO: export panel info to FDT */

	reg = (2 << BP_H_SHIFT);
	reg |= (41 << PW_H_SHIFT);
	reg |= (2 << FP_H_SHIFT);
	WRITE4(sc, DCU_HSYN_PARA, reg);

	reg = (2 << BP_V_SHIFT);
	reg |= (10 << PW_V_SHIFT);
	reg |= (2 << FP_V_SHIFT);
	WRITE4(sc, DCU_VSYN_PARA, reg);

	WRITE4(sc, DCU_BGND, 0);
	WRITE4(sc, DCU_DIV_RATIO, 30);

	reg = (INV_VS | INV_HS);
	WRITE4(sc, DCU_SYNPOL, reg);

	reg = (0x3 << LS_BF_VS_SHIFT);
	reg |= (0x78 << OUT_BUF_HIGH_SHIFT);
	reg |= (0 << OUT_BUF_LOW_SHIFT);
	WRITE4(sc, DCU_THRESHOLD, reg);

	/* Mask all the interrupts */
	WRITE4(sc, DCU_INT_MASK, 0xffffffff);

	/* Setup first layer */
	reg = (sc->sc_info.fb_width | (sc->sc_info.fb_height << 16));
	WRITE4(sc, DCU_CTRLDESCLn_1(0), reg);
	WRITE4(sc, DCU_CTRLDESCLn_2(0), 0x0);
	WRITE4(sc, DCU_CTRLDESCLn_3(0), sc->sc_info.fb_pbase);
	reg = (BPP24 << BPP_SHIFT);
	reg |= EN_LAYER;
	reg |= (0xFF << TRANS_SHIFT); /* completely opaque */
	WRITE4(sc, DCU_CTRLDESCLn_4(0), reg);
	WRITE4(sc, DCU_CTRLDESCLn_5(0), 0xffffff);
	WRITE4(sc, DCU_CTRLDESCLn_6(0), 0x0);
	WRITE4(sc, DCU_CTRLDESCLn_7(0), 0x0);
	WRITE4(sc, DCU_CTRLDESCLn_8(0), 0x0);
	WRITE4(sc, DCU_CTRLDESCLn_9(0), 0x0);

	/* Enable DCU in normal mode */
	reg = READ4(sc, DCU_DCU_MODE);
	reg &= ~(DCU_MODE_M << DCU_MODE_S);
	reg |= (DCU_MODE_NORMAL << DCU_MODE_S);
	reg |= (RASTER_EN);
	WRITE4(sc, DCU_DCU_MODE, reg);
	WRITE4(sc, DCU_UPDATE_MODE, READREG);

	return (0);
}

static int
dcu_attach(device_t dev)
{
	struct dcu_softc *sc;
	int err;

	sc = device_get_softc(dev);

	if (bus_alloc_resources(dev, dcu_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Memory interface */
	sc->bst = rman_get_bustag(sc->res[0]);
	sc->bsh = rman_get_bushandle(sc->res[0]);

	/* Setup interrupt handler */
	err = bus_setup_intr(dev, sc->res[1], INTR_TYPE_BIO | INTR_MPSAFE,
	    NULL, dcu_intr, sc, &sc->ih);
	if (err) {
		device_printf(dev, "Unable to alloc interrupt resource.\n");
		return (ENXIO);
	}

	/* Bypass timing control (used for raw lcd panels) */
	tcon_bypass();

	sc->sc_info.fb_width = DISPLAY_WIDTH;
	sc->sc_info.fb_height = DISPLAY_HEIGHT;
	sc->sc_info.fb_stride = sc->sc_info.fb_width * 3;
	sc->sc_info.fb_bpp = sc->sc_info.fb_depth = 24;
	sc->sc_info.fb_size = sc->sc_info.fb_height * sc->sc_info.fb_stride;
	sc->sc_info.fb_vbase = (intptr_t)contigmalloc(sc->sc_info.fb_size,
	    M_DEVBUF, M_ZERO, 0, ~0, PAGE_SIZE, 0);
	sc->sc_info.fb_pbase = (intptr_t)vtophys(sc->sc_info.fb_vbase);

#if 0
	printf("%dx%d [%d]\n", sc->sc_info.fb_width, sc->sc_info.fb_height,
	    sc->sc_info.fb_stride);
	printf("pbase == 0x%08x\n", sc->sc_info.fb_pbase);
#endif

	memset((int8_t *)sc->sc_info.fb_vbase, 0x0, sc->sc_info.fb_size);

	dcu_init(sc);

	sc->sc_info.fb_name = device_get_nameunit(dev);

	/* Ask newbus to attach framebuffer device to me. */
	sc->sc_fbd = device_add_child(dev, "fbd", device_get_unit(dev));
	if (sc->sc_fbd == NULL)
		device_printf(dev, "Can't attach fbd device\n");

	if (device_probe_and_attach(sc->sc_fbd) != 0) {
		device_printf(sc->dev, "Failed to attach fbd device\n");
	}

	return (0);
}

static struct fb_info *
dcu4_fb_getinfo(device_t dev)
{
	struct dcu_softc *sc = device_get_softc(dev);

	return (&sc->sc_info);
}

static device_method_t dcu_methods[] = {
	DEVMETHOD(device_probe,		dcu_probe),
	DEVMETHOD(device_attach,	dcu_attach),

	/* Framebuffer service methods */
	DEVMETHOD(fb_getinfo,		dcu4_fb_getinfo),
	{ 0, 0 }
};

static driver_t dcu_driver = {
	"fb",
	dcu_methods,
	sizeof(struct dcu_softc),
};

static devclass_t dcu_devclass;

DRIVER_MODULE(fb, simplebus, dcu_driver, dcu_devclass, 0, 0);
