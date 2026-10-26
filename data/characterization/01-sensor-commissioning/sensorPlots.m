%% plot_ch5_figures.m
%  Commissioning figures for Chapter 5.
%  Put this file in the same folder as the CSVs and run it.
%  Outputs PNGs at 300 dpi, named fig_M1 to fig_M3.

clear; close all; clc;

%% ---- shared style ------------------------------------------------------
C.t      = [0.35 0.18 0.55;           % T1 purple
            0.85 0.33 0.10;           % T2 orange
            0.00 0.45 0.74;           % T3 blue
            0.20 0.60 0.25];          % T4 green
C.band   = [0.90 0.90 0.90];
LW       = 1.1;
FS       = 9;

set(groot,'defaultAxesFontSize',FS);
set(groot,'defaultAxesFontName','Times New Roman');
set(groot,'defaultTextFontName','Times New Roman');
set(groot,'defaultAxesBox','on');
set(groot,'defaultAxesXGrid','on');
set(groot,'defaultAxesYGrid','on');
set(groot,'defaultAxesGridAlpha',0.12);

labels = {'T1 (A0)','T2 (A1)','T3 (A2)','T4 (A3)'};

%% =======================================================================
%  FIGURE M1: thermistor calibration against the PT1000
%  (a) error vs PT1000 before   (b) after   (c) spread between the four
%% =======================================================================
U = readtable('thermistor_vs_pt1000_test.csv');
K = readtable('thermistor_vs_pt1000_calibrated.csv');

tU = U.elapsed_s/60;   ptU = U.PT1000_c;
tK = K.elapsed_s/60;   ptK = K.PT1000_c;
thU = [U.T1_A0_c U.T2_A1_c U.T3_A2_c U.T4_A3_c];
thK = [K.T1_A0_c K.T2_A1_c K.T3_A2_c K.T4_A3_c];

offU = thU - ptU;      offK = thK - ptK;
devU = thU - mean(thU,2);   % deviation from the four-thermistor mean
devK = thK - mean(thK,2);

f1 = figure('Units','centimeters','Position',[2 2 17 17]);
tl1 = tiledlayout(f1,3,1,'TileSpacing','compact','Padding','compact');

% ---- (a) error vs PT1000, before --------------------------------------
ax1 = nexttile; hold(ax1,'on');
for k = 1:4
    plot(tU,offU(:,k),'Color',C.t(k,:),'LineWidth',LW);
end
for k = 1:4   % mean error as a dashed line, same colour
    yline(mean(offU(:,k)),'--','Color',C.t(k,:),'LineWidth',1.0, ...
          'HandleVisibility','off');
end
yline(0,'k-','LineWidth',0.8,'HandleVisibility','off');
ylabel('Error (\circC)');
title(ax1,'(a) Error against the PT1000, before calibration','FontWeight','normal');
ax1.TitleHorizontalAlignment = 'left';
xlim([min(tU) max(tU)]); ylim([-1.2 1.2]);
hold(ax1,'off');

% ---- (b) error vs PT1000, after ---------------------------------------
ax2 = nexttile; hold(ax2,'on');
for k = 1:4
    plot(tK,offK(:,k),'Color',C.t(k,:),'LineWidth',LW);
end
for k = 1:4
    yline(mean(offK(:,k)),'--','Color',C.t(k,:),'LineWidth',1.0, ...
          'HandleVisibility','off');
end
yline(0,'k-','LineWidth',0.8,'HandleVisibility','off');
ylabel('Error (\circC)');
title(ax2,'(b) Error against the PT1000, after calibration','FontWeight','normal');
ax2.TitleHorizontalAlignment = 'left';
xlim([min(tK) max(tK)]); ylim([-1.2 1.2]);
hold(ax2,'off');

% ---- (c) agreement between the four thermistors -----------------------
ax3 = nexttile; hold(ax3,'on');
for k = 1:4
    plot(tU,devU(:,k),'Color',[C.t(k,:) 0.35],'LineWidth',0.8, ...
         'HandleVisibility','off');
end
for k = 1:4
    plot(tK,devK(:,k),'Color',C.t(k,:),'LineWidth',LW,'HandleVisibility','off');
end
yline(0,'k-','LineWidth',0.8,'HandleVisibility','off');
xlabel('Time (min)'); ylabel('Deviation (\circC)');
title(ax3,['(c) Agreement between the four thermistors, faint before ' ...
           'and solid after calibration'],'FontWeight','normal');
ax3.TitleHorizontalAlignment = 'left';
xlim([min(tK) max(tK)]); ylim([-0.7 0.7]);
hold(ax3,'off');

% one legend for the whole figure, placed beside the layout so that all
% three tiles stay the same width
lg1 = legend(ax1,labels,'Box','off');
lg1.Layout.Tile = 'east';

set(findall(f1,'Type','axes'),'Toolbar',[]);
exportgraphics(f1,'fig_M1_thermistor_calibration.png','Resolution',300);

fprintf('\n--- M1 ---\n');
fprintf('mean error vs PT1000, before: %+0.2f %+0.2f %+0.2f %+0.2f degC\n', mean(offU));
fprintf('mean error vs PT1000, after : %+0.2f %+0.2f %+0.2f %+0.2f degC\n', mean(offK));
fprintf('worst deviation from group mean: before %.2f, after %.2f degC\n', ...
        max(abs(devU(:))), max(abs(devK(:))));
fprintf('mean spread (max-min) : before %.2f, after %.2f degC\n', ...
        mean(max(thU,[],2)-min(thU,[],2)), mean(max(thK,[],2)-min(thK,[],2)));

%% =======================================================================
%  FIGURE M2: PT1000 ice bath, two independent baths
%  (a) both cooling curves, aligned on immersion
%  (b) settled readings of both, with the converter levels drawn
%% =======================================================================
I1 = readtable('pt1000_ice_bath_test1.csv');   % cubes
I2 = readtable('pt1000_ice_bath_test2.csv');   % crushed and drained

step = 4300/32768/3.85;            % one MAX31865 step on a PT1000, degC

% immersion = first sample where the reading falls by more than 0.5 degC
imm = @(T) find(diff(T) < -0.5, 1, 'first');
t1 = I1.time_s - I1.time_s(imm(I1.temperature_C));
t2 = I2.time_s - I2.time_s(imm(I2.temperature_C));

% settled windows: the last 60 s of run 1 and the last 100 s of run 2
s1 = t1 >= max(t1) - 60  & I1.temperature_C < 0.5;
s2 = t2 >= max(t2) - 100 & I2.temperature_C < 0.5;
m1 = mean(I1.temperature_C(s1));
m2 = mean(I2.temperature_C(s2));

cRun = [0.35 0.18 0.55;      % run 1
        0.85 0.33 0.10];     % run 2

f2 = figure('Units','centimeters','Position',[2 2 17 11]);
tiledlayout(f2,2,1,'TileSpacing','compact','Padding','compact');

% ---- (a) both cooling curves -------------------------------------------
ax4 = nexttile; hold(ax4,'on');
plot(t1,I1.temperature_C,'Color',cRun(1,:),'LineWidth',LW);
plot(t2,I2.temperature_C,'Color',cRun(2,:),'LineWidth',LW);
yline(0,'k--','LineWidth',0.9);
ylabel('Temperature (\circC)');
title(ax4,'(a) Both baths, aligned on the moment of immersion','FontWeight','normal');
ax4.TitleHorizontalAlignment = 'left';
legend({'Run 1, ice cubes','Run 2, crushed and drained','True ice point, 0 \circC'}, ...
       'Location','northeast','Box','off');
xlim([-20 max(t2)]);
hold(ax4,'off');

% ---- (b) settled readings, zoomed ---------------------------------------
q1 = t1 >= 150 & I1.temperature_C < 0.5;
q2 = t2 >= 150 & I2.temperature_C < 0.5;

ax5 = nexttile; hold(ax5,'on');
lv = unique([I1.temperature_C(q1); I2.temperature_C(q2)]);
for v = lv.'
    yline(v,'-','Color',[0.87 0.87 0.87],'LineWidth',0.6,'HandleVisibility','off');
end
% run 1 drawn wide and underneath, run 2 narrow on top, so where the two
% sit on the same converter level both are still visible
stairs(t1(q1),I1.temperature_C(q1),'Color',cRun(1,:),'LineWidth',2.2);
stairs(t2(q2),I2.temperature_C(q2),'Color',cRun(2,:),'LineWidth',1.0);
yline(0,'k--','LineWidth',1.0,'HandleVisibility','off');
text(152,0.012,'True ice point, 0 \circC','FontSize',FS-1, ...
     'VerticalAlignment','bottom');
xlabel('Time since immersion (s)'); ylabel('Temperature (\circC)');
title(ax5,sprintf(['(b) Settled readings, same colours as (a). Grey lines ' ...
      'are converter levels, %.3f \\circC apart'],step),'FontWeight','normal');
ax5.TitleHorizontalAlignment = 'left';
xlim([148 max(t2)+3]); ylim([-0.05 0.45]);
ax5.YGrid = 'off';
hold(ax5,'off');

set(findall(f2,'Type','axes'),'Toolbar',[]);
exportgraphics(f2,'fig_M2_pt1000_icebath.png','Resolution',300);

fprintf('\n--- M2 ice bath ---\n');
fprintf('run 1 (cubes)   : %+0.3f degC, sd %.3f, R %.2f ohm, lead %.2f ohm\n', ...
    m1, std(I1.temperature_C(s1)), mean(I1.resistance_ohm(s1)), mean(I1.resistance_ohm(s1))-1000);
fprintf('run 2 (crushed) : %+0.3f degC, sd %.3f, R %.2f ohm, lead %.2f ohm\n', ...
    m2, std(I2.temperature_C(s2)), mean(I2.resistance_ohm(s2)), mean(I2.resistance_ohm(s2))-1000);
fprintf('difference between the two baths: %.3f degC (one step = %.3f)\n', abs(m1-m2), step);

%% =======================================================================
%  FIGURE M3: gas sensors
%% =======================================================================
E = readtable('explorir_breath_test.csv');
B = readtable('scd30_breath_test.csv');
O = readtable('scd30_room_to_outside_test.csv');

tO = O.elapsed_s - O.elapsed_s(1);     % re-zero the outside test

f3 = figure('Units','centimeters','Position',[2 2 17 14]);
tiledlayout(f3,3,1,'TileSpacing','compact','Padding','compact');

ax6 = nexttile; hold(ax6,'on');
stairs(E.elapsed_s,E.co2_ppm,'Color',C.t(1,:),'LineWidth',LW);
ylabel('CO_2 (ppm)');
title(ax6,'(a) ExplorIR-M-E-100, breath response','FontWeight','normal');
ax6.TitleHorizontalAlignment = 'left';
xlim([0 max(E.elapsed_s)]);
hold(ax6,'off');

ax7 = nexttile; hold(ax7,'on');
yyaxis left
plot(B.elapsed_s,B.co2_ppm,'Color',C.t(3,:),'LineWidth',LW);
ylabel('CO_2 (ppm)'); ax7.YColor = C.t(3,:);
yyaxis right
plot(B.elapsed_s,B.rh_percent,'Color',C.t(4,:),'LineWidth',LW,'LineStyle','-');
ylabel('RH (%)'); ax7.YColor = C.t(4,:);
title(ax7,'(b) SCD30 on the same test, humidity peaks first','FontWeight','normal');
ax7.TitleHorizontalAlignment = 'left';
legend({'CO_2','Relative humidity'},'Location','northeast','Box','off');
xlim([0 max(B.elapsed_s)]);
hold(ax7,'off');

ax8 = nexttile; hold(ax8,'on');
fill([0 max(tO) max(tO) 0],[400 400 420 420],[0.80 0.88 0.80], ...
     'EdgeColor',[0.55 0.70 0.55],'FaceAlpha',0.9,'LineWidth',0.5);
plot(tO,O.co2_ppm,'Color',C.t(2,:),'LineWidth',LW);
xlabel('Time (s)'); ylabel('CO_2 (ppm)');
title(ax8,'(c) SCD30 carried from the room to outside','FontWeight','normal');
ax8.TitleHorizontalAlignment = 'left';
legend({'Typical outdoor air, 400 to 420 ppm','SCD30'}, ...
       'Location','northeast','Box','off');
xlim([0 max(tO)]); ylim([300 700]);
hold(ax8,'off');

set(findall(f3,'Type','axes'),'Toolbar',[]);
exportgraphics(f3,'fig_M3_gas_sensors.png','Resolution',300);

late = O.co2_ppm(tO >= 130);
fprintf('\n--- M3 gas ---\n');
fprintf('ExplorIR: base %d, peak %d ppm\n', E.co2_ppm(1), max(E.co2_ppm));
fprintf('SCD30 breath: base %d, peak %d ppm, RH %.0f to %.0f %%\n', ...
    B.co2_ppm(1), max(B.co2_ppm), B.rh_percent(1), max(B.rh_percent));
fprintf('SCD30 outside settled: mean %.0f ppm, range %d to %d\n', ...
    mean(late), min(late), max(late));

reset(groot);