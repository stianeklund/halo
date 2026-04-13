void input_abstraction_dispose(void)
{
  csmemset((void *)0x46b820, 0, 0xdc);
}

void input_abstraction_initialize(void)
{
  int i;
  char *entry;

  csmemset((void *)0x46b820, 0, 0xdc);
  entry = (char *)0x46b828;
  for (i = 0; i < 4; i++) {
    *(float *)(entry - 8) = 120.0f;
    *(float *)(entry - 4) = 60.0f;
    entry[0] = 0;
    entry[1] = 4;
    entry[2] = 2;
    entry[3] = 3;
    entry[4] = 1;
    entry[5] = 5;
    entry[6] = 6;
    entry[7] = 7;
    entry[8] = 0xc;
    entry[9] = 0xd;
    entry[10] = 0xe;
    entry[11] = 0xf;
    *(int16_t *)(entry + 12) = 0;
    entry[14] = 0;
    entry[15] = 0;
    *(char *)(0x46b8f4 + i) = (char)input_has_gamepad(i);
    entry += 0x18;
  }
  *(int *)0x46b8f0 = system_milliseconds();
  *(char *)0x46b8f8 = 1;
}

void input_abstraction_update(void)
{
  float lx, ly, rx, ry;
  float lmag, rmag;
  float lang, rang;
  float ldom, rdom;
  float fVar1, fVar2;
  int iVar10, iVar13, iVar14;
  int uVar15;
  bool bVar16;
  char cVar6;
  int16_t sVar9;
  uint16_t uVar7, uVar8;
  int local_18;
  int local_20;
  uint8_t *pbVar11;
  uint8_t *puVar12;

  iVar10 = ((int (*)(void))0x18e450)();
  ((void (*)(int *, int, int))0x19b210)((int *)(iVar10 + 0x110), 0, 0x80);

  for (local_18 = 0; local_18 <= 3; local_18++) {
    iVar10 = (int)input_get_gamepad_state(local_18);
    if (iVar10 == 0) {
      if (((char *)0x46b8f4)[local_18] != '\0') {
        cVar6 = ((char (*)(void))0xe43e0)();
        iVar10 = local_18;
        if (cVar6 == '\0') {
          iVar13 = ((int (*)(void))0x12a240)();
          bVar16 = iVar13 == 0;
          uVar15 = bVar16 ? 0xc : 0xd;
          cVar6 = (char)local_player_exists(local_18);
          if (cVar6 != '\x01')
            goto skip_disconnect_ui;
        } else {
          iVar14 = 0;
          for (iVar13 = 0; iVar13 < 4; iVar13++) {
            if (((char *)0x46b8f4)[iVar13] != '\0')
              iVar14++;
          }
          bVar16 = false;
          uVar15 = 0xd;
          if (iVar14 < 2) {
            sVar9 = player_ui_get_single_player_local_player_controller(0);
            if (sVar9 != local_18) {
              sVar9 = player_ui_get_single_player_local_player_controller(1);
              if (sVar9 != local_18) {
                sVar9 = player_ui_get_single_player_local_player_controller(2);
                if (sVar9 != local_18) {
                  sVar9 =
                    player_ui_get_single_player_local_player_controller(3);
                  if (sVar9 != local_18) {
                    cVar6 = ((char (*)(int))0xe0890)(local_18);
                    if (cVar6 == '\0')
                      iVar10 = -1;
                  }
                }
              }
            }
          } else {
            sVar9 = player_ui_get_single_player_local_player_controller(0);
            if (sVar9 != local_18) {
              sVar9 = player_ui_get_single_player_local_player_controller(1);
              if (sVar9 != local_18) {
                sVar9 = player_ui_get_single_player_local_player_controller(2);
                if (sVar9 != local_18) {
                  sVar9 =
                    player_ui_get_single_player_local_player_controller(3);
                  if (sVar9 != local_18) {
                    cVar6 = ((char (*)(int))0xe0890)(local_18);
                    if (cVar6 == '\0')
                      goto skip_disconnect_ui;
                  }
                }
              }
            }
          }
        }
        cVar6 = ((char (*)(void))0xf5640)();
        if (cVar6 != '\0')
          ((void (*)(void))0xf57a0)();
        ((void (*)(int, int, bool, bool))0xe4500)(uVar15, iVar10, bVar16,
                                                  bVar16);
      }
    skip_disconnect_ui:
      ((char *)0x46b8f4)[local_18] = 0;
    } else {
      iVar13 = local_18 * 0x18;
      uVar15 = *(uint16_t *)((char *)0x46b824 + local_18 * 6);
      ((uint8_t *)0x457098)[local_18] =
        *(uint8_t *)((char *)0x46b820 + local_18 * 6);
      ((uint16_t *)0x4570a8)[local_18] = (uint16_t)uVar15;

      /* normalize left stick via dominant-axis scaling */
      lang = (float)atan2((double)(int)*(int16_t *)(iVar10 + 0x22),
                          (double)(int)*(int16_t *)(iVar10 + 0x20));
      fVar1 = lang;
      ldom = fabsf(sinf(lang));
      fVar2 = fabsf(cosf(fVar1));
      if (ldom <= fVar2)
        ldom = fVar2;
      lx = (float)(int)*(int16_t *)(iVar10 + 0x20) * *(float *)0x280f80 *
           (*(float *)0x2573d8 / ldom);
      lx = lx < *(float *)0x255e94 ? -1.0f :
           lx > *(float *)0x2533c8 ? 1.0f :
                                     lx;
      ly = (float)(int)*(int16_t *)(iVar10 + 0x22) * *(float *)0x280f80 *
           (*(float *)0x2573d8 / ldom);
      ly = ly < *(float *)0x255e94 ? -1.0f :
           ly > *(float *)0x2533c8 ? 1.0f :
                                     ly;

      /* normalize right stick */
      rang = (float)atan2((double)(int)*(int16_t *)(iVar10 + 0x26),
                          (double)(int)*(int16_t *)(iVar10 + 0x24));
      fVar2 = rang;
      rdom = fabsf(sinf(rang));
      fVar1 = fabsf(cosf(fVar2));
      if (rdom <= fVar1)
        rdom = fVar1;
      rx = (float)(int)*(int16_t *)(iVar10 + 0x24) * *(float *)0x280f80 *
           (*(float *)0x2573d8 / rdom);
      rx = rx < *(float *)0x255e94 ? -1.0f :
           rx > *(float *)0x2533c8 ? 1.0f :
                                     rx;
      ry = (float)(int)*(int16_t *)(iVar10 + 0x26) * *(float *)0x280f80 *
           (*(float *)0x2573d8 / rdom);
      ry = ry < *(float *)0x255e94 ? -1.0f :
           ry > *(float *)0x2533c8 ? 1.0f :
                                     ry;

      /* copy button state with remapping */
      pbVar11 = (uint8_t *)((char *)0x46b829 + iVar13);
      puVar12 = (uint8_t *)((char *)0x46b881 + local_18 * 0x1c);
      puVar12[-1] = *(uint8_t *)(pbVar11[-1] + 0x10 + iVar10);
      puVar12[0] = *(uint8_t *)(pbVar11[0] + 0x10 + iVar10);
      puVar12[1] = *(uint8_t *)(pbVar11[1] + 0x10 + iVar10);
      puVar12[2] = *(uint8_t *)(pbVar11[2] + 0x10 + iVar10);
      puVar12[3] = *(uint8_t *)(pbVar11[3] + 0x10 + iVar10);
      puVar12[4] = *(uint8_t *)(pbVar11[4] + 0x10 + iVar10);
      pbVar11 += 6;
      puVar12 += 6;
      puVar12[-1] = *(uint8_t *)(pbVar11[-1] + 0x10 + iVar10);
      puVar12[0] = *(uint8_t *)(pbVar11[0] + 0x10 + iVar10);
      puVar12[1] = *(uint8_t *)(pbVar11[1] + 0x10 + iVar10);
      puVar12[2] = *(uint8_t *)(pbVar11[2] + 0x10 + iVar10);
      puVar12[3] = *(uint8_t *)(pbVar11[3] + 0x10 + iVar10);
      puVar12[4] = *(uint8_t *)(pbVar11[4] + 0x10 + iVar10);

      /* axis snapping for presets 2 and 3 */
      if (((char *)0x46b834)[local_18 * 0xc] == 2 ||
          ((char *)0x46b834)[local_18 * 0xc] == 3) {
        uVar7 = (uint16_t)(*(float *)0x2533c0 <= ly ? 0 : 2);
        uVar8 = (uint16_t)(*(float *)0x2533c0 <= ry ? 0 : 2);
        lmag = sqrtf(lx * lx + ly * ly);
        rmag = sqrtf(rx * rx + ry * ry);
        fVar1 = fabsf(
          lang - *(float *)((char *)0x280f84 +
                            (int16_t)((lx < *(float *)0x2533c0) | uVar7) * 4));
        if (*(float *)0x281148 <= fVar1) {
          if (fabsf(lx) <= fabsf(ly)) {
            local_20 = *(float *)0x2533c0 <= ly ? 1 : -1;
            lx = 0.0f;
            ly = (float)local_20 * lmag;
          } else {
            local_20 = *(float *)0x2533c0 <= lx ? 1 : -1;
            ly = 0.0f;
            lx = (float)local_20 * lmag;
          }
        } else if (fabsf(lang) < *(float *)0x254a58 ||
                   *(float *)0x26af48 < fabsf(lang)) {
          local_20 = *(float *)0x2533c0 <= lx ? 1 : -1;
          lx = (float)local_20 * lmag;
          local_20 = *(float *)0x2533c0 <= ly ? 1 : -1;
          ly = (float)local_20 * lmag *
               (*(float *)0x2573d8 - fVar1 * *(float *)0x281140);
        } else {
          local_20 = *(float *)0x2533c0 <= ly ? 1 : -1;
          ly = (float)local_20 * lmag;
          local_20 = *(float *)0x2533c0 <= lx ? 1 : -1;
          lx = (float)local_20 * lmag *
               (*(float *)0x2573d8 - fVar1 * *(float *)0x281140);
        }
        fVar2 = fabsf(
          rang - *(float *)((char *)0x280f84 +
                            (int16_t)((rx < *(float *)0x2533c0) | uVar8) * 4));
        if (*(float *)0x281138 <= fVar2) {
          if (fabsf(ry) < fabsf(rx)) {
            local_20 = *(float *)0x2533c0 <= rx ? 1 : -1;
            ry = 0.0f;
            rx = (float)local_20 * rmag;
          } else {
            local_20 = *(float *)0x2533c0 <= ry ? 1 : -1;
            rx = 0.0f;
            ry = (float)local_20 * rmag;
          }
        } else if (fabsf(rang) < *(float *)0x254a58 ||
                   *(float *)0x26af48 < fabsf(rang)) {
          local_20 = *(float *)0x2533c0 <= rx ? 1 : -1;
          rx = (float)local_20 * rmag;
          local_20 = *(float *)0x2533c0 <= ry ? 1 : -1;
          ry = (float)local_20 * rmag *
               (*(float *)0x2573d8 - fVar2 * *(float *)0x281140);
        } else {
          local_20 = *(float *)0x2533c0 <= ry ? 1 : -1;
          ry = (float)local_20 * rmag;
          local_20 = *(float *)0x2533c0 <= rx ? 1 : -1;
          rx = (float)local_20 * rmag *
               (*(float *)0x2573d8 - fVar2 * *(float *)0x281140);
        }
      }

      /* inversion flags */
      cVar6 = ((char *)0x46b836)[iVar13];
      if (cVar6 == '\0' && ((char *)0x46b837)[iVar13] != '\0')
        cVar6 = ((char (*)(void))0xce8c0)();

      /* output axes per joystick preset */
      switch (((char *)0x46b834)[local_18 * 0xc]) {
      case 0:
        ((float *)0x46b890)[local_18 * 7] =
          *(char *)(iVar10 + 0x1a) != '\0' ? 1.0f :
          *(char *)(iVar10 + 0x1b) != '\0' ? -1.0f :
                                             -lx;
        ((float *)0x46b88c)[local_18 * 7] =
          *(char *)(iVar10 + 0x18) != '\0' ? 1.0f :
          *(char *)(iVar10 + 0x19) != '\0' ? -1.0f :
                                             ly;
        ((float *)0x46b894)[local_18 * 7] = -rx;
        ((float *)0x46b898)[local_18 * 7] =
          cVar6 == '\0' ? *(float *)0x2533c8 * ry : *(float *)0x255e94 * ry;
        ((char *)0x46b8f4)[local_18] = 1;
        break;
      case 1:
        ((float *)0x46b894)[local_18 * 7] =
          *(char *)(iVar10 + 0x1a) != '\0' ? 1.0f :
          *(char *)(iVar10 + 0x1b) != '\0' ? -1.0f :
                                             -lx;
        {
          float fv = *(float *)0x2533c8;
          if (*(char *)(iVar10 + 0x18) == '\0') {
            if (*(char *)(iVar10 + 0x19) == '\0') {
              fv = cVar6 != '\0' ? *(float *)0x255e94 * ly :
                                   *(float *)0x2533c8 * ly;
            } else if (cVar6 == '\0') {
              fv = *(float *)0x255e94;
            }
          } else if (cVar6 != '\0') {
            fv = *(float *)0x255e94;
          }
          ((float *)0x46b898)[local_18 * 7] = fv;
        }
        ((float *)0x46b88c)[local_18 * 7] = ry;
        ((float *)0x46b890)[local_18 * 7] = -rx;
        ((char *)0x46b8f4)[local_18] = 1;
        break;
      case 2:
        ((float *)0x46b894)[local_18 * 7] =
          *(char *)(iVar10 + 0x1a) != '\0' ? 1.0f :
          *(char *)(iVar10 + 0x1b) != '\0' ? -1.0f :
                                             -lx;
        ((float *)0x46b88c)[local_18 * 7] =
          *(char *)(iVar10 + 0x18) != '\0' ? 1.0f :
          *(char *)(iVar10 + 0x19) != '\0' ? -1.0f :
                                             ly;
        ((float *)0x46b890)[local_18 * 7] = -rx;
        ((float *)0x46b898)[local_18 * 7] =
          cVar6 == '\0' ? *(float *)0x2533c8 * ry : *(float *)0x255e94 * ry;
        ((char *)0x46b8f4)[local_18] = 1;
        break;
      case 3:
        ((float *)0x46b890)[local_18 * 7] =
          *(char *)(iVar10 + 0x1a) != '\0' ? 1.0f :
          *(char *)(iVar10 + 0x1b) != '\0' ? -1.0f :
                                             -lx;
        {
          float fv = *(float *)0x2533c8;
          if (*(char *)(iVar10 + 0x18) == '\0') {
            if (*(char *)(iVar10 + 0x19) == '\0') {
              fv = cVar6 != '\0' ? *(float *)0x255e94 * ly :
                                   *(float *)0x2533c8 * ly;
            } else if (cVar6 == '\0') {
              fv = *(float *)0x255e94;
            }
          } else if (cVar6 != '\0') {
            fv = *(float *)0x255e94;
          }
          ((float *)0x46b898)[local_18 * 7] = fv;
        }
        ((float *)0x46b88c)[local_18 * 7] = ry;
        ((float *)0x46b894)[local_18 * 7] = -rx;
        ((char *)0x46b8f4)[local_18] = 1;
        break;
      default:
        error(2, "unknown joystick preset");
        ((char *)0x46b8f4)[local_18] = 1;
        break;
      }
    }
  }
}
