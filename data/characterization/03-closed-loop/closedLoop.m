%% plot_ch7_figures.m
%  Closed loop thermal figures for Chapter 7.
%  Put this file in the same folder as the four CSVs and run it.
%  Outputs three PNGs at 300 dpi.
%
%    fig_C1_regulation.png    Test 1, holding 37/39 with a lid disturbance
%    fig_C2_setpoint_sweep.png  Test 2, 39 and 43 C plateaus, two wells
%    fig_C3_heated_base.png   Tests 3 and 4, heated base and plate evenness

clear; close all; clc;

%% ---- shared style ------------------------------------------------------
C.cham  = [0.00 0.45 0.74];      % chamber air
C.glass = [0.85 0.33 0.10];      % lid glass
C.well  = [0.20 0.60 0.25];      % well / centre well
C.well2 = [0.00 0.55 0.55];      % second well
C.side  = [0.35 0.18 0.55];      % cold corner
C.base  = [0.60 0.40 0.10];      % base glass
C.amb   = [0.55 0.55 0.55];      % room
C.sp    = [0.30 0.30 0.30];      % setpoint lines
C.band  = [0.88 0.88 0.88];      % one converter step
LW      = 1.0;
FS      = 8;

set(groot,'defaultAxesFontSize',FS);
set(groot,'defaultAxesFontName','Times New Roman');
set(groot,'defaultTextFontName','Times New Roman');
set(groot,'defaultAxesBox','on');
set(groot,'defaultAxesXGrid','on');
set(groot,'defaultAxesYGrid','on');
set(groot,'defaultAxesGridAlpha',0.12);

STEP = 0.034;                    % one MAX31865 step on a PT1000, degC

% every channel is plotted exactly as logged, with no filtering

%% ---- load --------------------------------------------------------------
T1 = readtable('control_test1.csv');
T2 = readtable('control_test2.csv');
T3 = readtable('heated_base_test1.csv');
T4 = readtable('well_evenness.csv');

tLid   = 11110;                  % Test 1, chamber opened
tStep  = 3405;                   % Test 2, chamber setpoint 39 -> 43
S1 = [6000 10800];               % Test 1 steady window
P39 = [2205 3405];               % Test 2 plateau at 39
P43 = [10774 11974];             % Test 2 plateau at 43

%% =======================================================================
%  FIGURE C1: Test 1, regulation at 37/39 and the lid disturbance
%% =======================================================================
f1 = figure('Units','centimeters','Position',[2 2 17 15.5]);
tiledlayout(f1,3,1,'TileSpacing','compact','Padding','compact');

% ---- (a) the whole run --------------------------------------------------
ax = nexttile; hold(ax,'on');
tm = T1.t_s/60;
plot(tm,T1.T_cham ,'Color',C.cham ,'LineWidth',LW);
plot(tm,T1.T_glass,'Color',C.glass,'LineWidth',LW);
plot(tm,T1.T_well ,'Color',C.well ,'LineWidth',LW);
plot(tm,T1.T_side ,'Color',C.side ,'LineWidth',LW);
plot(tm,T1.T_amb  ,'Color',C.amb  ,'LineWidth',LW);
yline(37,'--','Color',C.sp,'LineWidth',0.8,'HandleVisibility','off');
yline(39,'--','Color',C.sp,'LineWidth',0.8,'HandleVisibility','off');
xline(tLid/60,'k:','LineWidth',1.0,'HandleVisibility','off');
ylim([16 42]);
text(tLid/60-2,41,'lid opened','FontSize',FS-1,'FontName','Times New Roman', ...
     'HorizontalAlignment','right');
xlabel('Run time (min)'); ylabel('Temperature (\circC)');
title(ax,'(a) Three hour run at 37\circC chamber and 39\circC glass','FontWeight','normal');
ax.TitleHorizontalAlignment = 'left';
xlim([0 max(tm)]);
lg = legend({'Chamber air (PT1000)','Lid glass','Water in well','Cold corner','Room'}, ...
            'Box','off','FontSize',FS-1,'NumColumns',5);
lg.Layout.Tile = 'north';
hold(ax,'off');

% ---- (b) regulation against the converter step -------------------------
% a 3 min slice of the steady window, so the individual steps are readable
W  = [S1(1) S1(1)+180];
m  = T1.t_s >= W(1) & T1.t_s <= W(2);
ts = (T1.t_s(m) - W(1))/60;
ec = T1.T_cham(m) - 37;
ax = nexttile; hold(ax,'on');
lv = unique(T1.T_cham(T1.t_s>=S1(1) & T1.t_s<=S1(2))) - 37;   % every level it reaches
for k = 1:numel(lv)
    yline(lv(k),'-','Color',C.band,'LineWidth',0.8,'HandleVisibility','off');
end
stairs(ts,ec,'Color',C.cham,'LineWidth',LW);
yline(0,'--','Color',C.sp,'LineWidth',0.8,'HandleVisibility','off');
xlabel('Time into the steady window (min)'); ylabel('Chamber error (\circC)');
title(ax,sprintf('(b) 3 min of the steady window. The grey lines are the only %d values the converter returned in 80 min',numel(lv)),'FontWeight','normal');
text(0.02*max(ts),max(lv)-0.004,sprintf('80 min mean 37.028\\circC, std 0.038\\circC, one step = %.3f\\circC',STEP), ...
     'FontSize',FS-1,'FontName','Times New Roman');
ax.TitleHorizontalAlignment = 'left';
xlim([0 max(ts)]); ylim([min(lv)-0.02 max(lv)+0.02]);
hold(ax,'off');

% ---- (c) the lid disturbance --------------------------------------------
m  = T1.t_s >= tLid-180 & T1.t_s <= tLid+720;
ts = (T1.t_s(m) - tLid)/60;
ax = nexttile; hold(ax,'on');
yyaxis left
plot(ts,T1.T_cham(m),'Color',C.cham,'LineWidth',LW);
yline(37,'--','Color',C.sp,'LineWidth',0.8,'HandleVisibility','off');
ylabel('Temperature (\circC)'); ylim([36.4 37.3]);
ax.YAxis(1).Color = C.cham;
yyaxis right
stairs(ts,T1.heat_pct(m),'Color',C.side,'LineWidth',LW);
ylabel('Heater duty (%)'); ylim([26 34]);
ax.YAxis(2).Color = C.side;
xline(0,'k:','LineWidth',1.0);
xlabel('Time from opening the chamber (min)');
title(ax,'(c) Recovery after the chamber was opened','FontWeight','normal');
ax.TitleHorizontalAlignment = 'left';
xlim([min(ts) max(ts)]);
hold(ax,'off');

set(findall(f1,'Type','axes'),'Toolbar',[]);
exportgraphics(f1,'fig_C1_regulation.png','Resolution',300);

%% =======================================================================
%  FIGURE C2: Test 2, setpoint sweep with two wells
%% =======================================================================
f2 = figure('Units','centimeters','Position',[2 2 17 11.5]);
tiledlayout(f2,2,1,'TileSpacing','compact','Padding','compact');

% ---- (a) the whole run --------------------------------------------------
ax = nexttile; hold(ax,'on');
tm = T2.t_s/60;
plot(tm,T2.T_cham ,'Color',C.cham ,'LineWidth',LW);
plot(tm,T2.T_glass,'Color',C.glass,'LineWidth',LW);
plot(tm,T2.T_wellC,'Color',C.well ,'LineWidth',LW);
plot(tm,T2.T_wellE,'Color',C.well2,'LineWidth',LW);
plot(tm,T2.T_amb  ,'Color',C.amb  ,'LineWidth',LW);
yline(37,':','Color',C.sp,'LineWidth',1.0,'HandleVisibility','off');
xline(tStep/60,'k--','LineWidth',0.8,'HandleVisibility','off');
ylim([16 52]);
text(tStep/60+2,50,'chamber 39 \rightarrow 43\circC','FontSize',FS-1,'FontName','Times New Roman');
text(max(tm)-2,35.4,'37\circC target','FontSize',FS-1,'FontName','Times New Roman','HorizontalAlignment','right');
xlabel('Run time (min)'); ylabel('Temperature (\circC)');
title(ax,'(a) Lid off, glass held at 48\circC, chamber stepped from 39 to 43\circC','FontWeight','normal');
ax.TitleHorizontalAlignment = 'left';
xlim([0 max(tm)]);
lg = legend({'Chamber air','Lid glass','Centre well','Edge well','Room'}, ...
            'Box','off','FontSize',FS-1,'NumColumns',5);
lg.Layout.Tile = 'north';
hold(ax,'off');

% ---- (b) the two plateaus as bars --------------------------------------
avg = @(T,w,c) mean(T.(c)(T.t_s>=w(1) & T.t_s<=w(2)),'omitnan');
V = [avg(T2,P39,'T_cham') avg(T2,P39,'T_wellC') avg(T2,P39,'T_wellE');
     avg(T2,P43,'T_cham') avg(T2,P43,'T_wellC') avg(T2,P43,'T_wellE')];
ax = nexttile; hold(ax,'on');
b = bar(V,'EdgeColor','none','BarWidth',0.75);
b(1).FaceColor = C.cham; b(2).FaceColor = C.well; b(3).FaceColor = C.well2;
yline(37,'--','Color',C.sp,'LineWidth',1.0,'HandleVisibility','off');
for i = 1:2
    for j = 1:3
        text(b(j).XEndPoints(i),V(i,j)+0.4,sprintf('%.1f',V(i,j)), ...
             'HorizontalAlignment','center','FontSize',FS-1,'FontName','Times New Roman');
    end
end
text(2.42,37.6,'37\circC target','FontSize',FS-1,'FontName','Times New Roman', ...
     'HorizontalAlignment','right');
set(ax,'XTick',1:2,'XTickLabel',{'Chamber setpoint 39\circC','Chamber setpoint 43\circC'});
ylabel('Temperature (\circC)'); ylim([0 48]);
title(ax,'(b) Plateau values. Raising the air moves both wells and does not close the spread','FontWeight','normal');
ax.TitleHorizontalAlignment = 'left';
legend({'Chamber air','Centre well','Edge well'},'Location','northwest','Box','off', ...
       'FontSize',FS-1,'NumColumns',3);
hold(ax,'off');

set(findall(f2,'Type','axes'),'Toolbar',[]);
exportgraphics(f2,'fig_C2_setpoint_sweep.png','Resolution',300);

%% =======================================================================
%  FIGURE C3: Tests 3 and 4, the heated base
%% =======================================================================
f3 = figure('Units','centimeters','Position',[2 2 17 15.5]);
tiledlayout(f3,3,1,'TileSpacing','compact','Padding','compact');

tB20 = 2767; tB45 = 8233;        % Test 3, base duty changes

% ---- (a) Test 3 temperatures -------------------------------------------
ax = nexttile; hold(ax,'on');
tm = T3.t_s/60;
plot(tm,T3.T_cham ,'Color',C.cham,'LineWidth',LW);
plot(tm,T3.T_wellC,'Color',C.well,'LineWidth',LW);
plot(tm,T3.T_base ,'Color',C.base,'LineWidth',LW);
yline(37,':','Color',C.sp,'LineWidth',1.0,'HandleVisibility','off');
xline(tB20/60,'k--','LineWidth',0.8,'HandleVisibility','off');
xline(tB45/60,'k--','LineWidth',0.8,'HandleVisibility','off');
xline(2392/60,'k:','LineWidth',0.8,'HandleVisibility','off');
ylim([20 50]);
text(tB20/60+1.5,48.6,'base 20%','FontSize',FS-1,'FontName','Times New Roman');
text(tB45/60+1.5,48.6,'base 45%','FontSize',FS-1,'FontName','Times New Roman');
text(2392/60-1.5,22.2,'probe knocked','FontSize',FS-1,'FontName','Times New Roman', ...
     'HorizontalAlignment','right');
xlabel('Run time (min)'); ylabel('Temperature (\circC)');
title(ax,'(a) Heated base, chamber held at 39\circC throughout','FontWeight','normal');
ax.TitleHorizontalAlignment = 'left';
xlim([0 max(tm)]);
lg = legend({'Chamber air','Water in well','Base glass'}, ...
            'Box','off','FontSize',FS-1,'NumColumns',3);
lg.Layout.Tile = 'north';
hold(ax,'off');

% ---- (b) Test 3 duties, the authority transfer -------------------------
ax = nexttile; hold(ax,'on');
stairs(tm,T3.heat_pct,'Color',C.cham,'LineWidth',LW);
stairs(tm,T3.base_pct,'Color',C.base,'LineWidth',LW);
xline(tB20/60,'k--','LineWidth',0.8,'HandleVisibility','off');
xline(tB45/60,'k--','LineWidth',0.8,'HandleVisibility','off');
xlabel('Run time (min)'); ylabel('Duty (%)');
title(ax,'(b) The wall heaters give up authority as the base takes over','FontWeight','normal');
ax.TitleHorizontalAlignment = 'left';
xlim([0 max(tm)]); ylim([0 100]);
legend({'Wall heaters (closed loop)','Base (open loop)'},'Location','northwest', ...
       'Box','off','FontSize',FS-1);
hold(ax,'off');

% ---- (c) Test 4 evenness ------------------------------------------------
ax = nexttile; hold(ax,'on');
tm = T4.t_s/60;
plot(tm,T4.T_cham,'Color',C.cham,'LineWidth',LW);
plot(tm,T4.T_H12 ,'Color',C.side,'LineWidth',LW);
plot(tm,T4.T_E8  ,'Color',C.well,'LineWidth',LW);
yline(37,':','Color',C.sp,'LineWidth',1.0,'HandleVisibility','off');
ylim([28 42]);
text(max(tm)-2,37.6,'37\circC target','FontSize',FS-1,'FontName','Times New Roman', ...
     'HorizontalAlignment','right');
xlabel('Run time (min)'); ylabel('Temperature (\circC)');
title(ax,'(c) Two wells with the base at 30%: H12 at the corner, E8 near the centre','FontWeight','normal');
ax.TitleHorizontalAlignment = 'left';
xlim([0 max(tm)]);
legend({'Chamber air','Well H12 (corner)','Well E8 (centre)'},'Location','southeast', ...
       'Box','off','FontSize',FS-1);
hold(ax,'off');

set(findall(f3,'Type','axes'),'Toolbar',[]);
exportgraphics(f3,'fig_C3_heated_base.png','Resolution',300);

disp('Done: fig_C1_regulation.png, fig_C2_setpoint_sweep.png, fig_C3_heated_base.png');