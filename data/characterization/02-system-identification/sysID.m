%% plot_ch6_figures.m
%  Thermal system identification figures for Chapter 6.
%  Put this file in the same folder as heaters_test1.csv and glass_test1.csv
%  and run it. Outputs three PNGs at 300 dpi.
%
%    fig_T1_heater_step.png        wall heater test
%    fig_T2_glass_step.png         glass test
%    fig_T3_model_vs_measured.png  Chapter 3 prediction against measurement

clear; close all; clc;

%% ---- shared style ------------------------------------------------------
C.cham  = [0.00 0.45 0.74];      % chamber air, blue
C.cham2 = [0.40 0.72 0.92];      % chamber air, second step
C.glass = [0.85 0.33 0.10];      % glass, orange
C.glas2 = [0.95 0.62 0.38];      % glass, second step
C.well  = [0.20 0.60 0.25];      % water in well, green
C.side  = [0.35 0.18 0.55];      % cold corner, purple
C.amb   = [0.55 0.55 0.55];      % room, grey
C.fit   = [0.00 0.00 0.00];      % identified model, black
C.pred  = [0.80 0.00 0.40];      % Chapter 3 prediction, magenta
LW      = 1.0;
FS      = 8;

set(groot,'defaultAxesFontSize',FS);
set(groot,'defaultAxesFontName','Times New Roman');
set(groot,'defaultTextFontName','Times New Roman');
set(groot,'defaultAxesBox','on');
set(groot,'defaultAxesXGrid','on');
set(groot,'defaultAxesYGrid','on');
set(groot,'defaultAxesGridAlpha',0.12);

% every channel is plotted exactly as logged, with no filtering

%% ---- identified parameters (from the analysis in the chapter) ----------
P.cham  = struct('K',51.0,'tau',775,'th',0);     % wall heater duty to chamber air
P.glass = struct('K',38.7,'tau',370,'th',0);     % glass duty to glass
P.cross = struct('K',16.6,'tau',640,'th',100);   % glass duty to chamber air

% Chapter 3 analytical predictions
M.cham  = struct('K',128,'tau',594);
M.glass = struct('K', 73,'tau',158);

% step instants, read from the duty columns
tH = struct('s10',3814,'s20',4236,'off',6766);
tG = struct('s0',2,'s10',332,'s20',2914,'off',5450);

fopdt = @(t,K,tau,th,du) K*du*(1 - exp(-max(t-th,0)/tau)).*(t>=th);

%% ---- load --------------------------------------------------------------
H = readtable('heaters_test1.csv');
G = readtable('glass_test1.csv');

%% =======================================================================
%  FIGURE T1: wall heater step test
%% =======================================================================
f1 = figure('Units','centimeters','Position',[2 2 17 15.5]);
tiledlayout(f1,3,1,'TileSpacing','compact','Padding','compact');

% ---- (a) whole run ------------------------------------------------------
ax = nexttile; hold(ax,'on');
tm = H.t_s/60;
plot(tm,H.T_cham ,'Color',C.cham ,'LineWidth',LW);
plot(tm,H.T_glass,'Color',C.glass,'LineWidth',LW);
plot(tm,H.T_well ,'Color',C.well ,'LineWidth',LW);
plot(tm,H.T_side ,'Color',C.side ,'LineWidth',LW);
plot(tm,H.T_amb  ,'Color',C.amb  ,'LineWidth',LW);
xline(tH.s20/60,'k--','LineWidth',0.8,'HandleVisibility','off');
xline(tH.off/60,'k--','LineWidth',0.8,'HandleVisibility','off');
ylim([18 32]);                            % pinned before the labels go down
text(tH.s20/60+1.5,31.1,'10 \rightarrow 20%','FontSize',FS-1,'FontName','Times New Roman');
text(tH.off/60+1.5,31.1,'20 \rightarrow 0%' ,'FontSize',FS-1,'FontName','Times New Roman');
xlabel('Run time (min)');
ylabel('Temperature (\circC)');
title(ax,'(a) All channels, wall heaters stepped with the glass off','FontWeight','normal');
ax.TitleHorizontalAlignment = 'left';
xlim([min(tm) max(tm)]);
lg1 = legend({'Chamber air (PT1000)','Glass','Water in well','Cold corner','Room'}, ...
             'Box','off','FontSize',FS-1,'NumColumns',5);
lg1.Layout.Tile = 'north';
hold(ax,'off');

% ---- (b) the 10 to 20% step --------------------------------------------
[t,y] = segabs(H,'T_cham',tH.s20,tH.off-1);
ax = nexttile; hold(ax,'on');
plot(t,y,'Color',C.cham,'LineWidth',LW);
plot(t,y(1) + fopdt(t*60,P.cham.K,P.cham.tau,P.cham.th,0.10),'--','Color',C.fit,'LineWidth',1.2);
ylabel('Temperature (\circC)');
title(ax,'(b) Chamber air on the 10 to 20% step, against the identified model','FontWeight','normal');
ax.TitleHorizontalAlignment = 'left';
xlim([0 max(t)]);
legend({'Measured','51/(775s+1)'},'Location','southeast','Box','off','FontSize',FS-1);
hold(ax,'off');

% ---- (c) the cool-down --------------------------------------------------
[t,y] = segabs(H,'T_cham',tH.off,max(H.t_s));
ax = nexttile; hold(ax,'on');
plot(t,y,'Color',C.cham,'LineWidth',LW);
plot(t,y(1) + fopdt(t*60,P.cham.K,P.cham.tau,P.cham.th,-0.20),'--','Color',C.fit,'LineWidth',1.2);
xlabel('Time since step (min)');
ylabel('Temperature (\circC)');
title(ax,'(c) The 20 to 0% cool-down, same model, no refitting','FontWeight','normal');
ax.TitleHorizontalAlignment = 'left';
xlim([0 max(t)]);
legend({'Measured','51/(775s+1)'},'Location','northeast','Box','off','FontSize',FS-1);
hold(ax,'off');

set(findall(f1,'Type','axes'),'Toolbar',[]);
exportgraphics(f1,'fig_T1_heater_step.png','Resolution',300);

%% =======================================================================
%  FIGURE T2: glass step test
%% =======================================================================
f2 = figure('Units','centimeters','Position',[2 2 17 15.5]);
tiledlayout(f2,3,1,'TileSpacing','compact','Padding','compact');

% ---- (a) whole run ------------------------------------------------------
ax = nexttile; hold(ax,'on');
tm = G.t_s/60;
plot(tm,G.T_glass,'Color',C.glass,'LineWidth',LW);
plot(tm,G.T_cham ,'Color',C.cham ,'LineWidth',LW);
plot(tm,G.T_side ,'Color',C.side ,'LineWidth',LW);
plot(tm,G.T_amb  ,'Color',C.amb  ,'LineWidth',LW);
xline(tG.s10/60,'k--','LineWidth',0.8,'HandleVisibility','off');
xline(tG.s20/60,'k--','LineWidth',0.8,'HandleVisibility','off');
xline(tG.off/60,'k--','LineWidth',0.8,'HandleVisibility','off');
ylim([18 28.6]);                          % pinned before the labels go down
text(tG.s10/60+1.2,27.9,'0 \rightarrow 10%' ,'FontSize',FS-1,'FontName','Times New Roman');
text(tG.s20/60+1.2,27.9,'10 \rightarrow 20%','FontSize',FS-1,'FontName','Times New Roman');
text(tG.off/60+1.2,27.9,'20 \rightarrow 0%' ,'FontSize',FS-1,'FontName','Times New Roman');
xlabel('Run time (min)');
ylabel('Temperature (\circC)');
title(ax,'(a) All channels, glass stepped with the wall heaters off','FontWeight','normal');
ax.TitleHorizontalAlignment = 'left';
xlim([min(tm) max(tm)]);
lg2 = legend({'Glass','Chamber air (PT1000)','Cold corner','Room'}, ...
             'Box','off','FontSize',FS-1,'NumColumns',4);
lg2.Layout.Tile = 'north';
hold(ax,'off');

% ---- (b) glass channel, both steps overlaid ----------------------------
[t1,d1] = seg(G,'T_glass',tG.s10,tG.s20-1);
[t2,d2] = seg(G,'T_glass',tG.s20,tG.off-1);
ts = (0:1:2580)';
ax = nexttile; hold(ax,'on');
plot(t1,d1,'Color',C.glass,'LineWidth',LW);
plot(t2,d2,'Color',C.glas2,'LineWidth',LW);
plot(ts/60,fopdt(ts,P.glass.K,P.glass.tau,P.glass.th,0.10),'--','Color',C.fit,'LineWidth',1.2);
ylabel('Rise from plateau (\circC)');
title(ax,'(b) Glass on each 10% step, referred to its starting plateau','FontWeight','normal');
ax.TitleHorizontalAlignment = 'left';
xlim([0 43]); ylim([0 4.3]);
legend({'0 \rightarrow 10%','10 \rightarrow 20%','38.7/(370s+1)'}, ...
       'Location','southeast','Box','off','FontSize',FS-1);
hold(ax,'off');

% ---- (c) chamber cross-response, with the delay ------------------------
[t1,d1] = seg(G,'T_cham',tG.s10,tG.s20-1);
[t2,d2] = seg(G,'T_cham',tG.s20,tG.off-1);
ax = nexttile; hold(ax,'on');
plot(t1,d1,'Color',C.cham ,'LineWidth',LW);
plot(t2,d2,'Color',C.cham2,'LineWidth',LW);
plot(ts/60,fopdt(ts,P.cross.K,P.cross.tau,P.cross.th,0.10),'--','Color',C.fit,'LineWidth',1.2);
xline(P.cross.th/60,':','Color',C.fit,'LineWidth',1.0,'HandleVisibility','off');
text(P.cross.th/60+0.6,0.15,'\theta \approx 100 s','FontSize',FS-1,'FontName','Times New Roman');
xlabel('Time since step (min)');
ylabel('Rise from plateau (\circC)');
title(ax,'(c) Chamber air responding to the glass, same two steps','FontWeight','normal');
ax.TitleHorizontalAlignment = 'left';
xlim([0 43]); ylim([-0.1 2.0]);
legend({'0 \rightarrow 10%','10 \rightarrow 20%','16.6 e^{-100s}/(640s+1)'}, ...
       'Location','southeast','Box','off','FontSize',FS-1);
hold(ax,'off');

set(findall(f2,'Type','axes'),'Toolbar',[]);
exportgraphics(f2,'fig_T2_glass_step.png','Resolution',300);

%% =======================================================================
%  FIGURE T3: what Chapter 3 predicted against what was measured
%% =======================================================================
f3 = figure('Units','centimeters','Position',[2 2 17 10.5]);
tiledlayout(f3,2,1,'TileSpacing','compact','Padding','compact');

% ---- (a) chamber --------------------------------------------------------
[t,dy] = seg(H,'T_cham',tH.s20,tH.off-1);
ax = nexttile; hold(ax,'on');
plot(t,dy,'Color',C.cham,'LineWidth',LW);
plot(t,fopdt(t*60,P.cham.K,P.cham.tau,0,0.10),'--','Color',C.fit ,'LineWidth',1.2);
plot(t,fopdt(t*60,M.cham.K,M.cham.tau,0,0.10),'-.','Color',C.pred,'LineWidth',1.2);
ylabel('Rise (\circC)');
title(ax,'(a) Chamber air, 10% step in wall heater duty','FontWeight','normal');
ax.TitleHorizontalAlignment = 'left';
xlim([0 max(t)]); ylim([0 13.2]);
legend({'Measured','Identified, 51/(775s+1)','Chapter 3, 128/(594s+1)'}, ...
       'Location','northwest','Box','off','FontSize',FS-1);
hold(ax,'off');

% ---- (b) glass ----------------------------------------------------------
[t,dy] = seg(G,'T_glass',tG.s20,tG.off-1);
ax = nexttile; hold(ax,'on');
plot(t,dy,'Color',C.glass,'LineWidth',LW);
plot(t,fopdt(t*60,P.glass.K,P.glass.tau,0,0.10),'--','Color',C.fit ,'LineWidth',1.2);
plot(t,fopdt(t*60,M.glass.K,M.glass.tau,0,0.10),'-.','Color',C.pred,'LineWidth',1.2);
xlabel('Time since step (min)');
ylabel('Rise (\circC)');
title(ax,'(b) Glass, 10% step in glass duty','FontWeight','normal');
ax.TitleHorizontalAlignment = 'left';
xlim([0 max(t)]); ylim([0 7.8]);
legend({'Measured','Identified, 38.7/(370s+1)','Chapter 3, 73/(158s+1)'}, ...
       'Location','southeast','Box','off','FontSize',FS-1);
hold(ax,'off');

set(findall(f3,'Type','axes'),'Toolbar',[]);
exportgraphics(f3,'fig_T3_model_vs_measured.png','Resolution',300);

disp('Done: fig_T1_heater_step.png, fig_T2_glass_step.png, fig_T3_model_vs_measured.png');

%% ---- local functions ---------------------------------------------------
% one step, referred to its own starting plateau
function [t,dy] = seg(T,col,t0,t1)
    [t,y] = segabs(T,col,t0,t1);
    dy = y - mean(y(1:20),'omitnan');
end

% one step, in absolute temperature
function [t,y] = segabs(T,col,t0,t1)
    m = T.t_s >= t0 & T.t_s <= t1;
    t = (T.t_s(m) - t0)/60;
    y = T.(col)(m);
end