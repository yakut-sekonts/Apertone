"""Independent algebraic regressions for the Core ML export adaptation."""
from pathlib import Path
import sys
import unittest

import torch
from torch.nn import functional as F

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
from models.nafnet import Downsample2d, LinearDenoise


class DownsampleTests(unittest.TestCase):
    def test_matches_direct_convolution_and_checkpoint_layout(self):
        generator = torch.Generator().manual_seed(367)
        for channels in (1, 3, 32):
            layer = Downsample2d(channels, channels * 2).double()
            state = {"weight": torch.randn(channels * 2, channels, 2, 2,
                                          dtype=torch.float64, generator=generator),
                     "bias": torch.randn(channels * 2, dtype=torch.float64, generator=generator)}
            layer.load_state_dict(state, strict=True)
            self.assertEqual(set(layer.state_dict()), {"weight", "bias"})
            for height, width in ((2, 2), (8, 10), (9, 11), (10, 9)):
                with self.subTest(channels=channels, height=height, width=width):
                    image = torch.randn(1, channels, height, width,
                                        dtype=torch.float64, generator=generator)
                    original = image.clone()
                    with torch.inference_mode():
                        expected = F.conv2d(image, state["weight"], state["bias"], stride=2)
                        actual = layer(image)
                    self.assertEqual(actual.shape, (1, channels * 2, height // 2, width // 2))
                    torch.testing.assert_close(actual, expected, rtol=1e-12, atol=1e-12)
                    self.assertTrue(torch.equal(image, original))
                    for name, value in layer.state_dict().items():
                        self.assertTrue(torch.equal(value, state[name]))

    def test_spatial_and_channel_order(self):
        layer = Downsample2d(2, 8).double()
        with torch.no_grad():
            layer.weight.zero_()
            layer.bias.zero_()
            for channel in range(2):
                for y in range(2):
                    for x in range(2):
                        layer.weight[channel * 4 + y * 2 + x, channel, y, x] = 1
        image = torch.arange(2 * 6 * 8, dtype=torch.float64).reshape(1, 2, 6, 8)
        with torch.inference_mode():
            actual = layer(image)
        for channel in range(2):
            for y in range(2):
                for x in range(2):
                    self.assertTrue(torch.equal(actual[:, channel * 4 + y * 2 + x],
                                                image[:, channel, y::2, x::2]))


class UniformTileTests(unittest.TestCase):
    class BrightPrediction(torch.nn.Module):
        def forward(self, image):
            return torch.ones_like(image)

    def test_preserves_uniform_colors_without_a_brightness_threshold(self):
        model = LinearDenoise(self.BrightPrediction())
        for color in ((0, 0, 0), (.0001, .0001, .0001), (.18, .18, .18),
                      (.15, .4, .8), (1, 1, 1), (-.1, .5, 1.2)):
            with self.subTest(color=color):
                image = torch.tensor(color, dtype=torch.float32).reshape(1, 3, 1, 1)
                image = image.expand(1, 3, 16, 16).clone()
                self.assertTrue(torch.equal(model(image), image))

    def test_real_variation_still_reaches_prediction(self):
        model = LinearDenoise(self.BrightPrediction())
        image = torch.zeros(1, 3, 16, 16)
        image[0, 0, 5, 7] = 1e-7
        self.assertTrue(torch.equal(model(image), torch.ones_like(image)))

    def test_outside_domain_pixels_keep_all_three_channels(self):
        model = LinearDenoise(self.BrightPrediction())
        image = torch.zeros(1, 3, 16, 16)
        image[0, :, 5, 7] = torch.tensor([-.1, .5, .3])
        image[0, :, 8, 9] = torch.tensor([.2, .4, 1.1])
        actual = model(image)
        self.assertTrue(torch.equal(actual[0, :, 5, 7], image[0, :, 5, 7]))
        self.assertTrue(torch.equal(actual[0, :, 8, 9], image[0, :, 8, 9]))
        self.assertTrue(torch.equal(actual[0, :, 0, 0], torch.ones(3)))


if __name__ == "__main__":
    torch.set_num_threads(2)
    unittest.main()
