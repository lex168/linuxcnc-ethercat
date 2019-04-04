//
//    Copyright (C) 2018 Sascha Ittner <sascha.ittner@modusoft.de>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

#include "lcec.h"
#include "lcec_ax5805.h"
#include "lcec_ax5200.h"

typedef struct {
  int chanCount;

  hal_u32_t *fsoe_master_cmd;
  hal_u32_t *fsoe_master_crc0;
  hal_u32_t *fsoe_master_crc1;
  hal_u32_t *fsoe_master_connid;

  hal_u32_t *fsoe_slave_cmd;
  hal_u32_t *fsoe_slave_crc0;
  hal_u32_t *fsoe_slave_crc1;
  hal_u32_t *fsoe_slave_connid;

  hal_bit_t *fsoe_in_sto0;
  hal_bit_t *fsoe_in_sto1;

  unsigned int fsoe_master_cmd_os;
  unsigned int fsoe_master_crc0_os;
  unsigned int fsoe_master_crc1_os;
  unsigned int fsoe_master_connid_os;

  unsigned int fsoe_slave_cmd_os;
  unsigned int fsoe_slave_crc0_os;
  unsigned int fsoe_slave_crc1_os;
  unsigned int fsoe_slave_connid_os;

  unsigned int fsoe_in_sto0_os;
  unsigned int fsoe_in_sto0_bp;
  unsigned int fsoe_in_sto1_os;
  unsigned int fsoe_in_sto1_bp;

} lcec_ax5805_data_t;

static const lcec_pindesc_t slave_pins_1ch[] = {
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_master_cmd), "%s.%s.%s.fsoe-master-cmd" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_master_crc0), "%s.%s.%s.fsoe-master-crc" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_master_connid), "%s.%s.%s.fsoe-master-connid" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_slave_cmd), "%s.%s.%s.fsoe-slave-cmd" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_slave_crc0), "%s.%s.%s.fsoe-slave-crc" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_slave_connid), "%s.%s.%s.fsoe-slave-connid" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_in_sto0), "%s.%s.%s.fsoe-in-sto" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const lcec_pindesc_t slave_pins_2ch[] = {
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_master_cmd), "%s.%s.%s.fsoe-master-cmd" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_master_crc0), "%s.%s.%s.fsoe-master-crc-0" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_master_crc1), "%s.%s.%s.fsoe-master-crc-1" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_master_connid), "%s.%s.%s.fsoe-master-connid" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_slave_cmd), "%s.%s.%s.fsoe-slave-cmd" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_slave_crc0), "%s.%s.%s.fsoe-slave-crc-0" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_slave_crc1), "%s.%s.%s.fsoe-slave-crc-1" },
  { HAL_U32, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_slave_connid), "%s.%s.%s.fsoe-slave-connid" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_in_sto0), "%s.%s.%s.fsoe-in-sto-0" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_ax5805_data_t, fsoe_in_sto1), "%s.%s.%s.fsoe-in-sto-1" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static const LCEC_CONF_FSOE_T fsoe_conf_1ch = {
  .slave_data_len = 2,
  .master_data_len = 2,
  .data_channels = 1
};

static const LCEC_CONF_FSOE_T fsoe_conf_2ch = {
  .slave_data_len = 2,
  .master_data_len = 2,
  .data_channels = 2
};

void lcec_ax5805_chancount(struct lcec_slave *slave);
void lcec_ax5805_read(struct lcec_slave *slave, long period);

int lcec_ax5805_chan_count(struct lcec_slave *slave) {
  lcec_master_t *master = slave->master;
  struct lcec_slave *ax5n_slave;

  // try to find corresponding ax5n
  ax5n_slave = lcec_slave_by_index(master, slave->index - 1);
  if (ax5n_slave == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "%s.%s: unable to find corresponding AX5nxx with index %d.\n", master->name, slave->name, slave->index - 1);
    return -EINVAL;
  }

  // check for AX52xx
  if (ax5n_slave->proc_init == lcec_ax5200_init) {
    return 2;
  }

  return 1;
}

int lcec_ax5805_preinit(struct lcec_slave *slave) {
  int chanCount;

  chanCount = lcec_ax5805_chan_count(slave);
  if (chanCount < 0) {
    return chanCount;
  }

  if (chanCount >= 2) {
    slave->fsoeConf = &fsoe_conf_2ch;
    slave->pdo_entry_count = 10;
  } else {
    slave->fsoeConf = &fsoe_conf_1ch;
    slave->pdo_entry_count = 7;
  }

  return 0;
}

int lcec_ax5805_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_ax5805_data_t *hal_data;
  int err;
  const lcec_pindesc_t *slave_pins;

  // initialize callbacks
  slave->proc_read = lcec_ax5805_read;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_ax5805_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_ax5805_data_t));
  slave->hal_data = hal_data;

  // get number of channels
  hal_data->chanCount = lcec_ax5805_chan_count(slave);
  if (hal_data->chanCount < 0) {
    return hal_data->chanCount;
  }

  // initialize POD entries
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xE700, 0x01, &hal_data->fsoe_master_cmd_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xE700, 0x02, &hal_data->fsoe_master_connid_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xE600, 0x01, &hal_data->fsoe_slave_cmd_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xE600, 0x02, &hal_data->fsoe_slave_connid_os, NULL);

  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xE700, 0x03, &hal_data->fsoe_master_crc0_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xE600, 0x03, &hal_data->fsoe_slave_crc0_os, NULL);
  LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6640, 0x00, &hal_data->fsoe_in_sto0_os, &hal_data->fsoe_in_sto0_bp);

  if (hal_data->chanCount >= 2) {
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xE700, 0x04, &hal_data->fsoe_master_crc1_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0xE600, 0x04, &hal_data->fsoe_slave_crc1_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6E40, 0x00, &hal_data->fsoe_in_sto1_os, &hal_data->fsoe_in_sto1_bp);

    slave_pins = slave_pins_2ch;
  } else {
    slave_pins = slave_pins_1ch;
  }

  // export pins
  if ((err = lcec_pin_newf_list(hal_data, slave_pins, LCEC_MODULE_NAME, master->name, slave->name)) != 0) {
    return err;
  }

  return 0;
}

void lcec_ax5805_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_ax5805_data_t *hal_data = (lcec_ax5805_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;

  copy_fsoe_data(slave, hal_data->fsoe_slave_cmd_os, hal_data->fsoe_master_cmd_os);

  *(hal_data->fsoe_master_cmd) = EC_READ_U8(&pd[hal_data->fsoe_master_cmd_os]);
  *(hal_data->fsoe_master_connid) = EC_READ_U16(&pd[hal_data->fsoe_master_connid_os]);
  *(hal_data->fsoe_slave_cmd) = EC_READ_U8(&pd[hal_data->fsoe_slave_cmd_os]);
  *(hal_data->fsoe_slave_connid) = EC_READ_U16(&pd[hal_data->fsoe_slave_connid_os]);

  *(hal_data->fsoe_master_crc0) = EC_READ_U16(&pd[hal_data->fsoe_master_crc0_os]);
  *(hal_data->fsoe_slave_crc0) = EC_READ_U16(&pd[hal_data->fsoe_slave_crc0_os]);
  *(hal_data->fsoe_in_sto0) = EC_READ_BIT(&pd[hal_data->fsoe_in_sto0_os], hal_data->fsoe_in_sto0_bp);

  if (hal_data->chanCount >= 2) {
    *(hal_data->fsoe_master_crc1) = EC_READ_U16(&pd[hal_data->fsoe_master_crc1_os]);
    *(hal_data->fsoe_slave_crc1) = EC_READ_U16(&pd[hal_data->fsoe_slave_crc1_os]);
    *(hal_data->fsoe_in_sto1) = EC_READ_BIT(&pd[hal_data->fsoe_in_sto1_os], hal_data->fsoe_in_sto1_bp);
  }
}
