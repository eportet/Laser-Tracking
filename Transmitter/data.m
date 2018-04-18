start_index = 1000;
end_index = 15999;
size = end_index - start_index + 1;
M = csvread('test1.csv',start_index,0,[start_index 0 end_index 4]);

B = (M(:,5) == 0);
step = B .* M(:,4);
len = length(step);
hold = zeros(len,1);

v = 0;
for i = len:-1:1
    hold(i) = v;
    if step(i)
        v = step(i);
    end
end

T = M(:,4) + M(:,2)*4;
M_lpf = conv(M(:,2),ones(20,1)/20,'same');

P_lpf = conv(M(:,4),ones(20,1)/20,'same');

figure;
subplot(2,2,1);
plot(1:size,M(:,1));
title 'high freq x'
subplot(2,2,2);
plot(1:size,M(:,2),1:size,M_lpf);
title 'high freq y (low pass filtered)'
subplot(2,2,3);
plot(1:size,M(:,3),1:size,M(:,1)/4);
title 'low freq x'
subplot(2,2,4);
plot(1:size,M(:,4),1:size,hold, 1:size, T, 1:size, P_lpf);
title 'low freq y'


