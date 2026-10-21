"""Inference-only NAFNet SIDD width32, adapted for Core ML export.

Copyright (c) 2022 megvii-model. MIT; see resources/models/licenses/NAFNet-LICENSE.txt.
Source: NAFNet commit 2b4af71ebe098a92a75910c233a3965a3e93ede4.
Luma modifications: fixed architecture/input size, ordinary tensor LayerNorm
instead of custom autograd, explicit downsampling, no training/local-pooling
code, linear RGB wrapper.
"""
import torch
from torch import nn


class LayerNorm2d(nn.Module):
    def __init__(self, channels):
        super().__init__()
        self.weight = nn.Parameter(torch.ones(channels))
        self.bias = nn.Parameter(torch.zeros(channels))

    def forward(self, image):
        mean = image.mean(1, keepdim=True)
        variance = (image - mean).square().mean(1, keepdim=True)
        normalized = (image - mean) / torch.sqrt(variance + 1e-6)
        return normalized * self.weight.view(1, -1, 1, 1) + self.bias.view(1, -1, 1, 1)


class SimpleGate(nn.Module):
    def forward(self, image):
        left, right = image.chunk(2, dim=1)
        return left * right


class Downsample2d(nn.Conv2d):
    """Checkpoint-compatible 2x2/stride-2 convolution without that Core ML op.

    The native Float32 CPU implementation on macOS 27 corrupts some border
    samples for the direct operation. Gather the four spatial phases explicitly
    and use a 1x1 convolution with the same coefficients and bias instead.
    """
    def __init__(self, channels, output_channels):
        super().__init__(channels, output_channels, 2, stride=2)

    def forward(self, image):
        # Discard incomplete patches just as a valid stride-2 convolution does.
        height, width = image.shape[-2] // 2 * 2, image.shape[-1] // 2 * 2
        patches = torch.cat([image[:, :, y:height:2, x:width:2]
                             for y in range(2) for x in range(2)], dim=1)
        weight = self.weight.permute(0, 2, 3, 1).reshape(self.out_channels, -1, 1, 1)
        return torch.nn.functional.conv2d(patches, weight, self.bias)


class NAFBlock(nn.Module):
    def __init__(self, channels):
        super().__init__()
        expanded = channels * 2
        self.conv1 = nn.Conv2d(channels, expanded, 1)
        self.conv2 = nn.Conv2d(expanded, expanded, 3, padding=1, groups=expanded)
        self.conv3 = nn.Conv2d(channels, channels, 1)
        self.sca = nn.Sequential(nn.AdaptiveAvgPool2d(1), nn.Conv2d(channels, channels, 1))
        self.sg = SimpleGate()
        self.conv4 = nn.Conv2d(channels, expanded, 1)
        self.conv5 = nn.Conv2d(channels, channels, 1)
        self.norm1 = LayerNorm2d(channels)
        self.norm2 = LayerNorm2d(channels)
        self.beta = nn.Parameter(torch.zeros(1, channels, 1, 1))
        self.gamma = nn.Parameter(torch.zeros(1, channels, 1, 1))

    def forward(self, image):
        value = self.sg(self.conv2(self.conv1(self.norm1(image))))
        value = self.conv3(value * self.sca(value))
        residual = image + value * self.beta
        return residual + self.conv5(self.sg(self.conv4(self.norm2(residual)))) * self.gamma


class NAFNet(nn.Module):
    def __init__(self):
        super().__init__()
        channels = 32
        self.intro = nn.Conv2d(3, channels, 3, padding=1)
        self.ending = nn.Conv2d(channels, 3, 3, padding=1)
        self.encoders = nn.ModuleList()
        self.downs = nn.ModuleList()
        self.ups = nn.ModuleList()
        self.decoders = nn.ModuleList()
        for count in (2, 2, 4, 8):
            self.encoders.append(nn.Sequential(*[NAFBlock(channels) for _ in range(count)]))
            self.downs.append(Downsample2d(channels, channels * 2))
            channels *= 2
        self.middle_blks = nn.Sequential(*[NAFBlock(channels) for _ in range(12)])
        for count in (2, 2, 2, 2):
            self.ups.append(nn.Sequential(nn.Conv2d(channels, channels * 2, 1, bias=False), nn.PixelShuffle(2)))
            channels //= 2
            self.decoders.append(nn.Sequential(*[NAFBlock(channels) for _ in range(count)]))

    def forward(self, image):
        # Luma exports fixed 512x512 tensors, divisible by all four downsamplers.
        value = self.intro(image)
        skips = []
        for encoder, down in zip(self.encoders, self.downs):
            value = encoder(value)
            skips.append(value)
            value = down(value)
        value = self.middle_blks(value)
        for decoder, up, skip in zip(self.decoders, self.ups, reversed(skips)):
            value = decoder(up(value) + skip)
        return self.ending(value) + image


class LinearDenoise(nn.Module):
    def __init__(self, network):
        super().__init__()
        self.network = network

    def forward(self, image):
        # SIDD is display-referred sRGB, not a RAW/scene-linear training corpus.
        bounded = image.clamp(0, 1)
        encoded = torch.where(bounded <= 0.0031308, bounded * 12.92,
                              1.055 * bounded.clamp(min=1e-8).pow(1 / 2.4) - 0.055)
        prediction = self.network(encoded).clamp(0, 1)
        decoded = torch.where(prediction <= 0.04045, prediction / 12.92,
                              ((prediction + 0.055) / 1.055).pow(2.4))
        # Preserve the whole RGB pixel if any channel is outside the trained
        # domain, including negative working-space samples and HDR highlights.
        in_domain = ((image >= 0) & (image <= 1)).all(dim=1, keepdim=True)
        # A spatially uniform RGB tile contains no observed noise to remove.
        # The SIDD network can invent bright structure on black/near-black
        # constants. Preserve every uniform color exactly, without a brightness
        # threshold or suppressing real variation in dark photographs.
        uniform = (image == image[:, :, :1, :1]).all()
        return torch.where(in_domain & ~uniform, decoded, image)
