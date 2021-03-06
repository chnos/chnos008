
#include "core.h"

struct SYSTEM *sys_main_str_buf;

void CHNMain(void)
{
	uchar s[128];
	int i;
	UI_Timer *t_1sec;
	uint new_tsc[2];
	uint old_tsc[2];
	uint sec_clock[2];

	init_system();

	init_screen_i(system.ui.draw.sht.desktop_buf, system.ui.draw.sht.taskbar_buf, system.ui.draw.sht.mouse_buf);
	sheet_updown(system.ui.draw.sht.core, -1);

	sheet_updown(system.ui.draw.sht.desktop, 0);
	sheet_updown(system.ui.draw.sht.taskbar, 1);
	sheet_updown(system.ui.draw.sht.mouse, 2);

	t_1sec = timer_alloc();
	timer_init(t_1sec, &system.data.fifo.main, 1);
	timer_settime_millisec(t_1sec, 1000);

	system.ui.console.consoles[0].win = make_window("console", system.ui.console.org_xsize, system.ui.console.org_ysize, 10, 10, sheet_get_topheight(), false);
	system.ui.console.consoles[0].task = task_alloc();
	system.ui.console.consoles[0].task->tss.esp = (int)sys_memman_alloc(64 * 1024) + 64 * 1024;
	system.ui.console.consoles[0].task->tss.eip = (int)&console_main;
	system.ui.console.consoles[0].task->tss.es = 1 * 8;
	system.ui.console.consoles[0].task->tss.cs = 2 * 8;
	system.ui.console.consoles[0].task->tss.ss = 1 * 8;
	system.ui.console.consoles[0].task->tss.ds = 1 * 8;
	system.ui.console.consoles[0].task->tss.fs = 1 * 8;
	system.ui.console.consoles[0].task->tss.gs = 1 * 8;
	task_arguments(system.ui.console.consoles[0].task, 2, &system.ui.console.consoles[0]);
	task_run(system.ui.console.consoles[0].task, 2, 2);

	for(;;){
		io_cli();
		if(fifo32_status(&system.data.fifo.main) == 0){
			task_sleep(system.ui.task.main);
			io_sti();
		} else{
			i = fifo32_get(&system.data.fifo.main);
			if(i == 1){
				sprintf(s, "%02X%02X/%02X/%02X", readcmos(0x32), readcmos(0x09), readcmos(0x08), readcmos(0x07));
				putfonts_asc_sht_i(system.ui.draw.sht.taskbar, (system.data.info.boot.scrnx - (8 * 10)) - 4, 4, 0x000000, 0xffffff, s);
				sprintf(s, "%02X:%02X:%02X", readcmos(0x04), readcmos(0x02), readcmos(0x00));
				putfonts_asc_sht_i(system.ui.draw.sht.taskbar, (system.data.info.boot.scrnx - (8 * 9)) - 4, 4 + 16, 0x000000, 0xffffff, s);
				old_tsc[0] = new_tsc[0];
				old_tsc[1] = new_tsc[1];
				read_tsc(new_tsc);
				if(new_tsc[1] > old_tsc[1]){
					sec_clock[0] = new_tsc[0] - old_tsc[0];
					sec_clock[1] = new_tsc[1] - old_tsc[1];
				} else if(new_tsc[1] < old_tsc[1]){
					sec_clock[0] = new_tsc[0] - old_tsc[0] - 1;
					sec_clock[1] = (0xffffffff - old_tsc[1]) + new_tsc[1];
				}
				sprintf(s, "%08u%08u", sec_clock[0], sec_clock[1]);
				putfonts_asc_sht_i(system.ui.draw.sht.taskbar, (system.data.info.boot.scrnx - (8 * (10 + 1 + 3 + 21 + 1))) - 4, 20, 0x000000, 0xffffff, s);

				timer_settime_millisec(t_1sec, 1000);
			}
		}
	}
}

void KeyBoardControlTask(void)
{
	int i;
	uchar s[128];
	UI_KeyInfo dec_key;
	UI_Console *key_to;

	key_to = (UI_Console *)0;

	for(;;){
		if(fifo32_status(&system.data.fifo.keycmd) > 0 && system.io.keyboard.cmd_wait < 0){
			system.io.keyboard.cmd_wait = fifo32_get(&system.data.fifo.keycmd);
			wait_KBC_sendready();
			io_out8(KEYB_DATA, system.io.keyboard.cmd_wait);
		}
		io_cli();
		if(fifo32_status(&system.data.fifo.keyctrl) == 0){
			task_sleep(system.ui.task.keyctrl);
			io_sti();
		} else{
			i = fifo32_get(&system.data.fifo.keyctrl);
			if(0x100 <= i && i <= 0x1ff){
				decode_key(&dec_key, i - 0x100);
				if(dec_key.make && dec_key.c != 0){/*�ʏ핶��*/
					sprintf(s, "[ ]");
					s[1] = dec_key.c;
					putfonts_asc_sht_i(system.ui.draw.sht.taskbar, (system.data.info.boot.scrnx - (8 * (10 + 1 + 3))) - 4, 4, 0x000000, 0xffffff, s);
					if(key_to != 0) fifo32_put(&key_to->task->fifo, dec_key.c + CONSOLE_FIFO_START_KEYB);
				} else if(dec_key.make && dec_key.keycode == 0x3B && key_shift != 0){/*Shift + F1*/
					if(key_to != 0 && key_to->task->tss.ss0 != 0){
						cons_put_str(key_to, "\nBreak(key) :\n");
						io_cli();
						key_to->task->tss.eax = (int)&(key_to->task->tss.esp0);
						key_to->task->tss.eip = (int)asm_end_app;
						change_window_active(key_to->win, true);
						io_sti();
					}
				} else if(dec_key.make && dec_key.keycode == 0x0E){/*BackSpace*/
					if(key_to != 0) fifo32_put(&key_to->task->fifo, 0x0e + CONSOLE_FIFO_START_KEYB);
				} else if(dec_key.make && dec_key.keycode == 0x0f){/*Tab*/
					if(key_to == 0){
						key_to = &system.ui.console.consoles[0];
						change_window_active(key_to->win, true);
						fifo32_put(&key_to->task->fifo, CONSOLE_FIFO_CURSOR_START);
					} else{
						fifo32_put(&key_to->task->fifo, CONSOLE_FIFO_CURSOR_STOP);
						change_window_active(key_to->win, false);
						key_to = (UI_Console *)0;
					}
				} else if(dec_key.make && dec_key.keycode == 0x57){/*F11*/
					if(system.ui.draw.sht.ctrl.top > 3) sheet_updown(system.ui.draw.sht.ctrl.sheets[2], sheet_get_topheight());
				} else if(dec_key.make && dec_key.keycode == 0x1c){/*Enter*/
					if(key_to != 0) fifo32_put(&key_to->task->fifo, 0x0a + CONSOLE_FIFO_START_KEYB);
				}
			}
		}
	}
}

void MouseControlTask(void)
{
	int i, mx, my, scrool = 0;
	int j, x, y;
	int k;
	UI_Sheet *sht = 0;
	int mmx = -1, mmy = -1;
	bool win_move = false;
	uchar s[128];

	mx = system.data.info.boot.scrnx >> 1;
	my = system.data.info.boot.scrny >> 1;

	for(;;){
		io_cli();
		if(fifo32_status(&system.data.fifo.mousectrl) == 0){
			task_sleep(system.ui.task.mousectrl);
			io_sti();
		} else{
			i = fifo32_get(&system.data.fifo.mousectrl);
			if(0x100 <= i && i <= 0x1ff){
				if(decode_mouse(i - 0x100) != 0){
					mx += system.io.mouse.decode.x;
					my += system.io.mouse.decode.y;
					if(mx < 0) mx = 0;
					if(my < 0) my = 0;
					if(mx > system.data.info.boot.scrnx - 1) mx = system.data.info.boot.scrnx - 1;
					if(my > system.data.info.boot.scrny - 1) my = system.data.info.boot.scrny - 1;

					if(system.io.mouse.decode.scrool == 0xFFFFFFFF) scrool++;
					if(system.io.mouse.decode.scrool == 0x00000001) scrool--;

					sprintf(s, "[lcr](%4d,%4d,%04d)", mx, my, scrool);
					if((system.io.mouse.decode.btn & 0x01) != 0){
						s[1] = 'L';
						if(win_move == false){
							for(j = system.ui.draw.sht.ctrl.top - 1; j >= 1; j--){
								sht = system.ui.draw.sht.ctrl.sheets[j];
								x = mx - sht->vx0;
								y = my - sht->vy0;
								if(0 <= x && x < sht->bxsize && 0 <= y && y < sht->bysize){
									sheet_updown(sht, sheet_get_topheight());
									if(0 <= y && y <= 24){
										for(k = 0;k < MAX_WINDOWS;k++){
											if(system.ui.window.ctrl.winfos[k].win == sht) break;
										}
										if(k < MAX_WINDOWS && y <= 16 && (sht->bxsize - 44) <= x && x <= (sht->bxsize - 4) && system.ui.window.ctrl.winfos[k].task != 0){
											io_cli();
											system.ui.window.ctrl.winfos[k].task->tss.eax = (int)&(system.ui.window.ctrl.winfos[k].task->tss.esp0);
											system.ui.window.ctrl.winfos[k].task->tss.eip = (int)asm_end_app;
											change_window_active(&system.ui.window.ctrl.winfos[k], true);
											io_sti();
										}
										if(k < MAX_WINDOWS){
											win_move = true;
											mmx = mx;
											mmy = my;
										}
									}
									break;
								}
							}
						} else{
							x = mx - mmx;
							y = my - mmy;
							sheet_slide(sht, sht->vx0 + x, sht->vy0 + y);
							mmx = mx;
							mmy = my;
						}
					} else{
						win_move = false;
					}
					if((system.io.mouse.decode.btn & 0x02) != 0){
						s[3] = 'R';
					}
					if((system.io.mouse.decode.btn & 0x04) != 0){
						s[2] = 'C';
					}
					putfonts_asc_sht_i(system.ui.draw.sht.taskbar, (system.data.info.boot.scrnx - (8 * (10 + 1 + 3 + 21 + 1))) - 4, 4, 0x000000, 0xffffff, s);
					sheet_slide(system.ui.draw.sht.mouse, mx, my);
				}
			}
		}
	}
}
